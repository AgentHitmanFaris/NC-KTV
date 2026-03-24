from audio_separator.separator import Separator

sep = Separator(model_file_dir='D:/NC-KTV/models/uvr')

models_to_download = [
    "mel_band_roformer_karaoke_aufr33_viperx_sdr_10.1956.ckpt",
    "mel_band_roformer_karaoke_gabox_v2.ckpt"
]

for model in models_to_download:
    print(f"\n>>> Downloading: {model}")
    try:
        sep.download_model_files(model)
        print(f"    Done: {model}")
    except Exception as e:
        print(f"    ERROR: {e}")

print("\nAll done!")
