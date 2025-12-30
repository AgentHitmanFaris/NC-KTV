"""
Vocal removal using UVR models via audio-separator library
"""

from pathlib import Path
from typing import List, Optional, Callable
import logging

from audio_separator.separator import Separator

from utils.config import Config
from utils.gpu_detector import GPUDetector


class VocalRemover:
    """Remove vocals from audio using UVR models"""
    
    def __init__(self, config: Config):
        """Initialize vocal remover
        
        Args:
            config: Application configuration
        """
        self.config = config
        self.logger = logging.getLogger(__name__)
        
        # Initialize separator
        self.separator = None
        self._initialize_separator()
    
    def _initialize_separator(self):
        """Initialize audio-separator instance"""
        # Get configuration
        use_gpu = self.config.get('uvr.use_gpu', True)
        gpu_device = self.config.get('uvr.gpu_device', 0)
        output_dir = self.config.get('processing.output_dir', 'output')
        
        # Check GPU availability
        gpu_available = GPUDetector.is_cuda_available()
        
        if use_gpu and not gpu_available:
            self.logger.warning("GPU requested but not available, falling back to CPU")
            use_gpu = False
        
        # Create output directory
        Path(output_dir).mkdir(parents=True, exist_ok=True)
        
        # Initialize separator
        self.separator = Separator(
            log_level=logging.INFO,
            model_file_dir=self.config.get('uvr.models_path', 'models'),
            output_dir=output_dir,
            output_single_stem='Instrumental',  # We want instrumental only
            normalization_threshold=0.9,
            output_format='WAV',
            # device=device,  # Removed: Not supported in this version
            sample_rate=self.config.get('processing.sample_rate', 44100)
        )
        
        self.logger.info(f"Vocal remover initialized (GPU: {use_gpu})")
    
    def list_available_models(self) -> List[str]:
        """Get list of available UVR models
        
        Returns:
            List of model filenames
        """
        models_path = Path(self.config.get('uvr.models_path', 'models'))
        
        if not models_path.exists():
            return []
        
        # Look for .pth and .onnx files
        models = []
        for ext in ['*.pth', '*.onnx', '*.pt']:
            models.extend([m.name for m in models_path.glob(ext)])
        
        return sorted(models)
    
    def separate_vocals(
        self,
        audio_path: Path,
        model_name: Optional[str] = None,
        progress_callback: Optional[Callable[[float, str], None]] = None
    ) -> dict:
        """Separate vocals from audio
        
        Args:
            audio_path: Path to input audio file
            model_name: UVR model to use (None = use default)
            progress_callback: Optional callback(percentage, status_message)
        
        Returns:
            Dictionary with output file paths:
            {
                'instrumental': Path to instrumental track,
                'vocals': Path to vocals track (if saved),
                'model_used': Name of model used
            }
        
        Raises:
            FileNotFoundError: If audio file doesn't exist
            RuntimeError: If separation fails
        """
        audio_path = Path(audio_path)
        
        if not audio_path.exists():
            raise FileNotFoundError(f"Audio file not found: {audio_path}")
        
        # Determine model to use
        if model_name is None:
            model_name = self.config.get('uvr.default_model', 'UVR_MDXNET_KARA_2.onnx')
        
        self.logger.info(f"Starting vocal separation: {audio_path.name}")
        self.logger.info(f"Using model: {model_name}")
        
        # Load model
        try:
            self.separator.load_model(model_filename=model_name)
        except Exception as e:
            raise RuntimeError(f"Failed to load model '{model_name}': {e}")
        
        # Report progress
        if progress_callback:
            progress_callback(10, "Model loaded, starting separation...")
        
        # Perform separation
        try:
            output_files = self.separator.separate(str(audio_path))
            
            if progress_callback:
                progress_callback(100, "Separation complete")
            
            # Parse output files
            result = {
                'model_used': model_name,
                'instrumental': None,
                'vocals': None
            }
            
            for file_path in output_files:
                file_path = Path(file_path)
                if 'Instrumental' in file_path.name:
                    result['instrumental'] = file_path
                elif 'Vocals' in file_path.name:
                    result['vocals'] = file_path
            
            self.logger.info(f"Separation successful. Output: {result['instrumental']}")
            
            return result
            
        except Exception as e:
            self.logger.error(f"Separation failed: {e}")
            raise RuntimeError(f"Vocal separation failed: {e}")
    
    def get_model_info(self, model_name: str) -> dict:
        """Get information about a model
        
        Args:
            model_name: Model filename
        
        Returns:
            Dictionary with model info
        """
        models_path = Path(self.config.get('uvr.models_path', 'models'))
        model_path = models_path / model_name
        
        if not model_path.exists():
            return {'exists': False}
        
        # Determine model type
        if 'MDX' in model_name.upper():
            model_type = 'MDX-Net'
        elif 'VR' in model_name.upper():
            model_type = 'VR Architecture'
        else:
            model_type = 'Unknown'
        
        return {
            'exists': True,
            'name': model_name,
            'type': model_type,
            'size_mb': model_path.stat().st_size / (1024 * 1024),
            'path': model_path
        }
    
    def clear_cache(self):
        """Clear GPU cache if using CUDA"""
        GPUDetector.clear_cache()
