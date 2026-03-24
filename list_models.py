from audio_separator.separator import Separator

sep = Separator(model_file_dir='D:/NC-KTV/models/uvr')
models = sep.list_supported_model_files()

keywords = ['karaoke', 'bs-roformer-karaoke', 'bs_roformer_karaoke']
print("=== BS-RoFormer / Karaoke MDXC Models ===")
for name, info in models['MDXC'].items():
    nl = name.lower()
    fn = info['filename'].lower()
    if any(k in nl or k in fn for k in keywords):
        scores = info.get('scores', {})
        print(f"  {name}")
        print(f"    filename: {info['filename']}")
        print(f"    scores: {scores}")
        print()
