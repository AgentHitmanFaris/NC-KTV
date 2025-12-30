"""
GPU detection and management utilities
"""

import torch
from typing import Dict, Optional


class GPUDetector:
    """Detect and manage GPU availability for processing"""
    
    @staticmethod
    def is_cuda_available() -> bool:
        """Check if CUDA is available"""
        return torch.cuda.is_available()
    
    @staticmethod
    def get_device(prefer_gpu: bool = True, device_id: int = 0) -> torch.device:
        """Get PyTorch device
        
        Args:
            prefer_gpu: Prefer GPU if available
            device_id: CUDA device ID
        
        Returns:
            PyTorch device
        """
        if prefer_gpu and torch.cuda.is_available():
            return torch.device(f'cuda:{device_id}')
        return torch.device('cpu')
    
    @staticmethod
    def get_gpu_info(device_id: int = 0) -> Optional[Dict[str, any]]:
        """Get GPU information
        
        Args:
            device_id: CUDA device ID
        
        Returns:
            Dictionary with GPU info or None if not available
        """
        if not torch.cuda.is_available():
            return None
        
        props = torch.cuda.get_device_properties(device_id)
        
        return {
            'name': props.name,
            'total_memory_gb': props.total_memory / (1024 ** 3),
            'cuda_capability': f"{props.major}.{props.minor}",
            'device_id': device_id
        }
    
    @staticmethod
    def get_recommended_batch_size(device_id: int = 0) -> int:
        """Get recommended batch size based on GPU memory
        
        Args:
            device_id: CUDA device ID
        
        Returns:
            Recommended batch size
        """
        info = GPUDetector.get_gpu_info(device_id)
        
        if not info:
            return 1  # CPU fallback
        
        memory_gb = info['total_memory_gb']
        
        # Rough estimates for UVR models
        if memory_gb >= 8:
            return 8
        elif memory_gb >= 6:
            return 4
        elif memory_gb >= 4:
            return 2
        else:
            return 1
    
    @staticmethod
    def clear_cache():
        """Clear GPU cache"""
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
