"""
NC-KTV Binary Format Handler
Custom encrypted container for project files with chunked streaming
"""

import struct
import os
import json
from pathlib import Path
from typing import Dict, Any, BinaryIO
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from cryptography.hazmat.primitives import hashes
import zstandard as zstd
import logging

logger = logging.getLogger(__name__)

# Format constants
MAGIC_BYTES = b'NCTV'
FORMAT_VERSION = 1
CHUNK_SIZE = 65536  # 64KB chunks for streaming
SALT = b'NC-KTV-2025-SECURE-CONTAINER'  # Fixed salt for key derivation

# Flags
FLAG_COMPRESSED = 0x01
FLAG_ENCRYPTED = 0x02


class NCTVFormat:
    """Handler for NC-KTV encrypted binary format"""
    
    @staticmethod
    def _derive_key() -> bytes:
        """
        Derive AES-256 key using PBKDF2
        
        Security: Uses PBKDF2-HMAC-SHA256 with 100k iterations
        This makes brute-force attacks significantly harder
        """
        passphrase = b"NC-KTV-PROPRIETARY-FORMAT-V1-ENCRYPTION-KEY"
        
        kdf = PBKDF2HMAC(
            algorithm=hashes.SHA256(),
            length=32,  # 256 bits for AES-256
            salt=SALT,
            iterations=100000,  # Industry standard
        )
        
        return kdf.derive(passphrase)
    
    @staticmethod
    def pack(project: 'Project', output_path: Path) -> None:
        """
        Pack project into encrypted .nctv file using chunked streaming
        
        Args:
            project: Project instance to pack
            output_path: Path to save .nctv file
        
        Design:
            1. Compress metadata JSON only (ZSTD level 9)
            2. Stream media files in 64KB chunks (no compression)
            3. Encrypt each chunk individually to avoid RAM bottleneck
        """
        logger.info(f"Packing project to: {output_path}")
        
        # Generate encryption key
        key = NCTVFormat._derive_key()
        aesgcm = AESGCM(key)
        
        # Prepare metadata (compressed JSON)
        metadata = {
            'project_name': project.project_name,
            'source_file_name': project.source_file.name if project.source_file else None,
            'settings': project.settings.__dict__,
            'lyrics': project.lyrics.to_dict(),
            'created': project.created_date,
            'modified': project.modified_date
        }
        
        # Compress metadata (small, benefits from compression)
        compressor = zstd.ZstdCompressor(level=9)
        metadata_json = json.dumps(metadata, ensure_ascii=False)
        metadata_compressed = compressor.compress(metadata_json.encode('utf-8'))
        
        # Collect media files
        media_files = []
        if project.source_file and project.source_file.exists():
            media_files.append(('source', project.source_file))
        if project.instrumental_file and project.instrumental_file.exists():
            media_files.append(('instrumental', project.instrumental_file))
        if project.vocals_file and project.vocals_file.exists():
            media_files.append(('vocals', project.vocals_file))
        if project.output_video and project.output_video.exists():
            media_files.append(('output', project.output_video))
        
        # Write .nctv file
        with open(output_path, 'wb') as f:
            # Header
            f.write(MAGIC_BYTES)  # Magic: "NCTV"
            f.write(struct.pack('<I', FORMAT_VERSION))  # Version
            f.write(struct.pack('<I', FLAG_COMPRESSED | FLAG_ENCRYPTED))  # Flags
            
            # Metadata block
            NCTVFormat._write_encrypted_block(f, aesgcm, metadata_compressed, 'metadata')
            
            # Media files count
            f.write(struct.pack('<I', len(media_files)))
            
            # Media blocks (chunked streaming)
            for file_type, file_path in media_files:
                NCTVFormat._write_media_file(f, aesgcm, file_type, file_path)
        
        logger.info(f"Successfully packed {len(media_files)} media files")
    
    @staticmethod
    def _write_encrypted_block(f: BinaryIO, aesgcm: AESGCM, data: bytes, label: str) -> None:
        """Write an encrypted data block"""
        nonce = os.urandom(12)
        encrypted = aesgcm.encrypt(nonce, data, None)
        
        # Write: [size][nonce][encrypted_data]
        f.write(struct.pack('<I', len(encrypted)))
        f.write(nonce)
        f.write(encrypted)
        
        logger.debug(f"Wrote {label} block: {len(data)} -> {len(encrypted)} bytes")
    
    @staticmethod
    def _write_media_file(f: BinaryIO, aesgcm: AESGCM, file_type: str, file_path: Path) -> None:
        """
        Write media file using chunked streaming encryption
        
        Memory-efficient: Reads 64KB at a time, encrypts, writes immediately
        No compression: Media files are already compressed
        """
        file_size = file_path.stat().st_size
        logger.info(f"Writing {file_type}: {file_path.name} ({file_size / 1024 / 1024:.1f} MB)")
        
        # Write file header
        file_name = file_path.name.encode('utf-8')
        f.write(struct.pack('<I', len(file_name)))
        f.write(file_name)
        f.write(struct.pack('<I', len(file_type)))
        f.write(file_type.encode('utf-8'))
        f.write(struct.pack('<Q', file_size))  # 8 bytes for large files
        
        # Write encrypted chunks
        with open(file_path, 'rb') as source:
            while True:
                chunk = source.read(CHUNK_SIZE)
                if not chunk:
                    break
                
                # Encrypt chunk
                nonce = os.urandom(12)
                encrypted_chunk = aesgcm.encrypt(nonce, chunk, None)
                
                # Write: [chunk_size][nonce][encrypted_data]
                f.write(struct.pack('<I', len(encrypted_chunk)))
                f.write(nonce)
                f.write(encrypted_chunk)
    
    @staticmethod
    def unpack(input_path: Path) -> 'Project':
        """
        Unpack encrypted .nctv file using chunked streaming
        
        Args:
            input_path: Path to .nctv file
        
        Returns:
            Project instance
        
        Raises:
            ValueError: If file format is invalid
        """
        logger.info(f"Unpacking project from: {input_path}")
        
        if not input_path.exists():
            raise FileNotFoundError(f"File not found: {input_path}")
        
        # Generate decryption key
        key = NCTVFormat._derive_key()
        aesgcm = AESGCM(key)
        
        with open(input_path, 'rb') as f:
            # Read header
            magic = f.read(4)
            if magic != MAGIC_BYTES:
                raise ValueError("Invalid .nctv file: Wrong magic bytes")
            
            version = struct.unpack('<I', f.read(4))[0]
            if version != FORMAT_VERSION:
                raise ValueError(f"Unsupported version: {version}")
            
            flags = struct.unpack('<I', f.read(4))[0]
            
            # Read metadata block
            metadata_encrypted = NCTVFormat._read_encrypted_block(f, aesgcm)
            
            # Decompress metadata
            decompressor = zstd.ZstdDecompressor()
            metadata_json = decompressor.decompress(metadata_encrypted)
            metadata = json.loads(metadata_json.decode('utf-8'))
            
            # Create temporary directory for extracted files
            temp_dir = input_path.parent / f".nctv_temp_{os.getpid()}"
            temp_dir.mkdir(exist_ok=True)
            
            # Read media files
            media_count = struct.unpack('<I', f.read(4))[0]
            logger.info(f"Extracting {media_count} media files...")
            
            media_paths = {}
            for _ in range(media_count):
                file_type, file_path = NCTVFormat._read_media_file(f, aesgcm, temp_dir)
                media_paths[file_type] = file_path
        
        # Reconstruct project
        from core.project import Project
        project = Project()
        project.project_name = metadata['project_name']
        project.created_date = metadata['created']
        project.modified_date = metadata['modified']
        
        # Restore settings
        from core.project import ProjectSettings
        project.settings = ProjectSettings(**metadata['settings'])
        
        # Restore lyrics
        from sync.sync_data import LyricsData
        project.lyrics = LyricsData.from_dict(metadata['lyrics'])
        
        # Assign media paths
        project.source_file = media_paths.get('source')
        project.instrumental_file = media_paths.get('instrumental')
        project.vocals_file = media_paths.get('vocals')
        project.output_video = media_paths.get('output')
        
        logger.info("Successfully unpacked project")
        return project
    
    @staticmethod
    def _read_encrypted_block(f: BinaryIO, aesgcm: AESGCM) -> bytes:
        """Read and decrypt a data block"""
        size = struct.unpack('<I', f.read(4))[0]
        nonce = f.read(12)
        encrypted = f.read(size)
        
        return aesgcm.decrypt(nonce, encrypted, None)
    
    @staticmethod
    def _read_media_file(f: BinaryIO, aesgcm: AESGCM, output_dir: Path) -> tuple[str, Path]:
        """Read and decrypt media file using chunked streaming"""
        
        # Read file header
        name_len = struct.unpack('<I', f.read(4))[0]
        file_name = f.read(name_len).decode('utf-8')
        
        type_len = struct.unpack('<I', f.read(4))[0]
        file_type = f.read(type_len).decode('utf-8')
        
        file_size = struct.unpack('<Q', f.read(8))[0]
        
        logger.info(f"Extracting {file_type}: {file_name} ({file_size / 1024 / 1024:.1f} MB)")
        
        # Extract to temp directory
        output_path = output_dir / file_name
        
        # Read and decrypt chunks
        bytes_written = 0
        with open(output_path, 'wb') as out:
            while bytes_written < file_size:
                chunk_size = struct.unpack('<I', f.read(4))[0]
                nonce = f.read(12)
                encrypted_chunk = f.read(chunk_size)
                
                # Decrypt and write
                decrypted_chunk = aesgcm.decrypt(nonce, encrypted_chunk, None)
                out.write(decrypted_chunk)
                bytes_written += len(decrypted_chunk)
        
        return file_type, output_path
