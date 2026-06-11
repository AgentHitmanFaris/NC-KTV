#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include "../src/core/timeline/timeline_manager.h"
#include "../src/core/timeline/clip.h"
#include "../src/core/timeline/track.h"
#include "../src/core/audio/audio_reader.h"
#include "../src/core/audio/audio_engine.h"
#include "../src/core/audio/stem_resampler.h"
#include "../src/core/audio/stem_separator.h"
#include "../src/core/audio/render_engine.h"

using namespace ncktv;

void testTimecodeConversions() {
    std::cout << "[TEST] Running Timecode Conversions test...\n";
    
    TimelineManager manager;
    manager.setFps(30.0);

    // Test 1: 1 second in microseconds formatted to 30fps timecode
    qint64 oneSec = 1000000LL;
    QString t1 = manager.formatTimecode(oneSec);
    assert(t1 == "00:00:01:00");

    // Test 2: Half a second (15 frames) formatted
    qint64 halfSec = 500000LL;
    QString t2 = manager.formatTimecode(halfSec);
    assert(t2 == "00:00:00:15");

    // Test 3: Parse timecode string "00:01:05:15"
    // (1 min, 5 sec, 15 frames) = 60 + 5 + 0.5 = 65.5 seconds = 65,500,000 microseconds
    qint64 parsed = manager.parseTimecode("00:01:05:15");
    assert(parsed == 65500000LL);

    // Test 4: Parse brief timecodes
    qint64 brief1 = manager.parseTimecode("02:15"); // 2 sec, 15 frames = 2.5s
    assert(brief1 == 2500000LL);

    // Test 5: Frames translation
    qint64 frames = manager.timeToFrames(65500000LL);
    assert(frames == 1965LL); // 65.5 * 30 = 1965
    qint64 timeFromFrames = manager.framesToTime(1965LL);
    assert(timeFromFrames == 65500000LL);

    std::cout << "[PASS] Timecode Conversions test completed successfully.\n\n";
}

void testSnappingEngine() {
    std::cout << "[TEST] Running Snapping Engine test...\n";

    TimelineManager manager;
    manager.setFps(30.0);
    
    // Set playhead to exactly 10.0 seconds (10,000,000 microseconds)
    manager.setCurrentPlayheadTime(10000000LL);

    // Test 1: Snap to playhead
    // Target start time is 9.95s, threshold is 100ms (100,000 microseconds) -> Should snap to 10.0s
    qint64 snapResult1 = manager.checkSnapping("", 9950000LL, 100000LL);
    assert(snapResult1 == 10000000LL);

    // Target start is 9.85s (150ms away, outside 100ms threshold) -> Should NOT snap (returns 9.85s)
    qint64 snapResult2 = manager.checkSnapping("", 9850000LL, 100000LL);
    assert(snapResult2 == 9850000LL);

    // Test 2: Snap to clips on tracks
    QString trackId = manager.addTrack(Track::Audio, "Background Audio");
    TrackListModel* model = manager.trackListModel();
    Track* track = model->getTrackById(trackId);
    assert(track != nullptr);

    // Add clip at start = 2.0s (2,000,000 microseconds), duration = 5.0s (5,000,000 microseconds, end = 7.0s)
    Clip* c1 = new Clip("clip_a", Clip::Audio, 2000000LL, 5000000LL, track);
    bool added = track->addClip(c1);
    assert(added);

    // Snap to clip start: target is 1.98s, threshold 50ms -> Should snap to 2.0s
    qint64 snapResult3 = manager.checkSnapping("", 1980000LL, 50000LL);
    assert(snapResult3 == 2000000LL);

    // Snap to clip end: target is 7.02s, threshold 50ms -> Should snap to 7.0s
    qint64 snapResult4 = manager.checkSnapping("", 7020000LL, 50000LL);
    assert(snapResult4 == 7000000LL);

    // Test 3: Snap target clip END to an existing clip's boundary
    // We are dragging a clip ("clip_b") that is NOT yet on the timeline, but we simulate it
    // Dragged clip has duration = 3.0s (3,000,000 microseconds).
    // If we drag its start to 4.02s, its end lies at 7.02s. This is close to c1's end at 7.0s.
    // So the end of "clip_b" should snap to 7.0s, shifting its start to 7.0s - 3.0s = 4.0s!
    // We pass "clip_b" as the exclude ID, but we first register its duration by registering it temporarily
    Clip* c2 = new Clip("clip_b", Clip::Audio, 0LL, 3000000LL, track);
    
    // We bypass Track::addClip because of overlap, we just test the snapping math by letting checkSnapping see it
    // To let checkSnapping see it, we can put it in a separate track or temporarily add it. Let's add it to a new track!
    QString track2Id = manager.addTrack(Track::Audio, "Target Track");
    Track* track2 = model->getTrackById(track2Id);
    track2->addClip(c2);

    // Now check snapping for "clip_b" (duration 3s). Dragged start target is 4.02s, threshold 50ms.
    // Clip c1's end is at 7.0s.
    // Target start 4.02s -> Target end 7.02s.
    // Delta to c1 end is 20ms (< 50ms threshold) -> Snapped start should be 4.0s!
    qint64 snapResult5 = manager.checkSnapping("clip_b", 4020000LL, 50000LL);
    assert(snapResult5 == 4000000LL);

    std::cout << "[PASS] Snapping Engine test completed successfully.\n\n";
}

void testClipSplitting() {
    std::cout << "[TEST] Running Clip Splitting test...\n";

    TimelineManager manager;
    QString trackId = manager.addTrack(Track::Audio, "Voice Over");
    Track* track = manager.trackListModel()->getTrackById(trackId);
    
    // Clip: start = 10s (10,000,000us), dur = 10s (10,000,000us, end = 20s)
    Clip* clip = new Clip("orig_clip", Clip::Audio, 10000000LL, 10000000LL, track);
    clip->setSourceFile("vocal_track.wav");
    clip->setSourceStart(0LL);
    clip->setSourceDuration(10000000LL);
    track->addClip(clip);

    assert(track->clips().size() == 1);
    
    // Split the clip at 14s (14,000,000 microseconds)
    // Left segment should be 10s -> 14s (duration 4s)
    // Right segment should be 14s -> 20s (duration 6s)
    bool splitSuccess = manager.splitClip(trackId, "orig_clip", 14000000LL);
    assert(splitSuccess);

    // Verify track now has 2 clips
    const QList<Clip*>& clips = track->clips();
    assert(clips.size() == 2);

    // Clips are kept sorted by start time
    Clip* left = clips[0];
    Clip* right = clips[1];

    assert(left->startTime() == 10000000LL);
    assert(left->duration() == 4000000LL);
    assert(left->endTime() == 14000000LL);
    assert(left->sourceFile() == "vocal_track.wav");
    assert(left->sourceStart() == 0LL);
    assert(left->sourceDuration() == 4000000LL);

    assert(right->startTime() == 14000000LL);
    assert(right->duration() == 6000000LL);
    assert(right->endTime() == 20000000LL);
    assert(right->sourceFile() == "vocal_track.wav");
    assert(right->sourceStart() == 4000000LL);
    assert(right->sourceDuration() == 6000000LL);

    std::cout << "[PASS] Clip Splitting test completed successfully.\n\n";
}

void testJsonSaveAndLoad() {
    std::cout << "[TEST] Running JSON Save & Load test...\n";

    TimelineManager manager;
    manager.setFps(25.0);
    manager.setCurrentPlayheadTime(5000000LL); // 5s

    // Add audio track
    QString audTrackId = manager.addTrack(Track::Audio, "Instrumental");
    Track* audTrack = manager.trackListModel()->getTrackById(audTrackId);
    Clip* audioClip = new Clip("aud_1", Clip::Audio, 0LL, 120000000LL, audTrack); // 2 mins
    audioClip->setSourceFile("backing_track.wav");
    audTrack->addClip(audioClip);

    // Add lyrics track
    QString lyrTrackId = manager.addTrack(Track::Lyrics, "Subtitles");
    Track* lyrTrack = manager.trackListModel()->getTrackById(lyrTrackId);
    lyrTrack->setIsLocked(false);
    Clip* lyricClip = new Clip("lyr_1", Clip::Lyrics, 4000000LL, 3000000LL, lyrTrack); // 4s -> 7s
    lyricClip->setLyricText("Welcome to NC-KTV V2!");
    lyrTrack->addClip(lyricClip);

    // Save project
    QString savePath = QDir::tempPath() + "/test_project.nctv";
    std::cout << "[INFO] Saving project to: " << savePath.toStdString() << "\n";
    bool saved = manager.saveProject(savePath);
    assert(saved);

    // Clear the active project
    manager.clearProject();
    assert(manager.trackListModel()->tracks().isEmpty());
    assert(manager.currentPlayheadTime() == 0);
    assert(manager.totalDuration() == 0);

    // Reload the saved project
    bool loaded = manager.loadProject(savePath);
    assert(loaded);

    // Verify all states are precisely recovered
    assert(manager.fps() == 25.0);
    assert(manager.currentPlayheadTime() == 5000000LL);
    assert(manager.totalDuration() == 120000000LL); // Total duration matches backing track end (120s)

    TrackListModel* model = manager.trackListModel();
    assert(model->tracks().size() == 2);

    Track* t1 = model->tracks()[0];
    assert(t1->name() == "Instrumental");
    assert(t1->type() == Track::Audio);
    assert(t1->clips().size() == 1);
    assert(t1->clips()[0]->clipId() == "aud_1");
    assert(t1->clips()[0]->startTime() == 0LL);
    assert(t1->clips()[0]->duration() == 120000000LL);
    assert(t1->clips()[0]->sourceFile() == "backing_track.wav");

    Track* t2 = model->tracks()[1];
    assert(t2->name() == "Subtitles");
    assert(t2->type() == Track::Lyrics);
    assert(t2->clips().size() == 1);
    assert(t2->clips()[0]->clipId() == "lyr_1");
    assert(t2->clips()[0]->startTime() == 4000000LL);
    assert(t2->clips()[0]->duration() == 3000000LL);
    assert(t2->clips()[0]->lyricText() == "Welcome to NC-KTV V2!");

    // Clean up temporary file
    QFile::remove(savePath);

    std::cout << "[PASS] JSON Save & Load test completed successfully.\n\n";
}

void testSourceTrimmingAndOffsets() {
    std::cout << "[TEST] Running Source Trimming and Offsets test...\n";

    TimelineManager manager;
    manager.setFps(30.0);

    // Add video track
    QString vidTrackId = manager.addTrack(Track::Video, "Background Video");
    Track* vidTrack = manager.trackListModel()->getTrackById(vidTrackId);
    assert(vidTrack != nullptr);

    // Add clip with sourceStart offset: startTime = 2.0s, duration = 5.0s, sourceStart = 3.0s
    // (Total duration of the clip is 5.0s, starting at 2.0s on the timeline, reading from source starting at 3.0s)
    bool added = manager.addClipToTrackWithSourceStart(
        vidTrackId,
        "vid_trim_1",
        Track::Video,
        2000000LL,  // 2s
        5000000LL,  // 5s
        3000000LL,  // 3s sourceStart
        "trimmed_source.mp4"
    );
    assert(added);

    // Verify clip attributes
    assert(vidTrack->clips().size() == 1);
    Clip* clip = vidTrack->clips()[0];
    assert(clip->clipId() == "vid_trim_1");
    assert(clip->startTime() == 2000000LL);
    assert(clip->duration() == 5000000LL);
    assert(clip->sourceStart() == 3000000LL);
    assert(clip->sourceFile() == "trimmed_source.mp4");

    // Save project
    QString savePath = QDir::tempPath() + "/test_trim_project.nctv";
    bool saved = manager.saveProject(savePath);
    assert(saved);

    // Clear
    manager.clearProject();
    assert(manager.trackListModel()->tracks().isEmpty());

    // Load
    bool loaded = manager.loadProject(savePath);
    assert(loaded);

    // Verify properties loaded successfully
    TrackListModel* model = manager.trackListModel();
    assert(model->tracks().size() == 1);
    Track* loadedTrack = model->tracks()[0];
    assert(loadedTrack->clips().size() == 1);
    Clip* loadedClip = loadedTrack->clips()[0];

    assert(loadedClip->clipId() == "vid_trim_1");
    assert(loadedClip->startTime() == 2000000LL);
    assert(loadedClip->duration() == 5000000LL);
    assert(loadedClip->sourceStart() == 3000000LL);
    assert(loadedClip->sourceFile() == "trimmed_source.mp4");

    QFile::remove(savePath);
    std::cout << "[PASS] Source Trimming and Offsets test completed successfully.\n\n";
}


void testSyllableParser() {
    std::cout << "[TEST] Running Syllable Parser test...\n";

    Clip lyricClip("syl_clip", Clip::Lyrics, 10000000LL, 5000000LL); // 10s start, 5s duration
    
    // Syllable LRC tags format test
    lyricClip.parseLrcSyllables("Word1 <00:10.50> Word2 <00:11.00> Word3 <00:12.00>");
    
    QVariantList sylList = lyricClip.syllables();
    assert(sylList.size() == 3);
    
    QVariantMap s1 = sylList[0].toMap();
    assert(s1["text"].toString() == "Word1 ");
    assert(s1["relativeStart"].toLongLong() == 0LL);
    assert(s1["duration"].toLongLong() == 500000LL);
    
    QVariantMap s2 = sylList[1].toMap();
    assert(s2["text"].toString() == " Word2 ");
    assert(s2["relativeStart"].toLongLong() == 500000LL);
    assert(s2["duration"].toLongLong() == 500000LL);
    
    QVariantMap s3 = sylList[2].toMap();
    assert(s3["text"].toString() == " Word3 ");
    assert(s3["relativeStart"].toLongLong() == 1000000LL);
    assert(s3["duration"].toLongLong() == 1000000LL);

    std::cout << "[PASS] Syllable Parser test completed successfully.\n\n";
}

void testLoDPeakGeneration() {
    std::cout << "[TEST] Running LoD Peak Generation test...\n";

    AudioReader reader;
    // Create exactly 8192 stereo samples (4096 frames)
    std::vector<float> mockSamples(8192, 0.0f);
    
    // First frame has peak 0.5
    mockSamples[0] = 0.5f;
    mockSamples[1] = -0.5f;
    
    // Frame at index 300 has peak 0.8
    mockSamples[300 * 2] = 0.8f;
    mockSamples[300 * 2 + 1] = -0.8f;

    reader.setSamplesForTesting(mockSamples);

    const auto& p256 = reader.peaks256();
    const auto& p4096 = reader.peaks4096();

    // 4096 frames / 256 = 16 peak-pairs
    assert(p256.size() == 16);
    // 4096 frames / 4096 = 1 peak-pair
    assert(p4096.size() == 1);

    // Assert LOD 256 peaks
    assert(std::abs(p256[0].maxVal - 0.5f) < 0.001f);
    assert(std::abs(p256[0].minVal - (-0.5f)) < 0.001f);
    assert(std::abs(p256[1].maxVal - 0.8f) < 0.001f);
    assert(std::abs(p256[1].minVal - (-0.8f)) < 0.001f);

    // Assert LOD 4096 peak is the absolute maximum over all 4096 frames (0.8)
    assert(std::abs(p4096[0].maxVal - 0.8f) < 0.001f);
    assert(std::abs(p4096[0].minVal - (-0.8f)) < 0.001f);

    std::cout << "[PASS] LoD Peak Generation test completed successfully.\n\n";
}

void testAudioGainRamping() {
    std::cout << "[TEST] Running Audio Gain Ramping test...\n";

    TimelineManager manager;
    AudioEngine engine(&manager);

    // Add audio track
    QString trackId = manager.addTrack(Track::Audio, "Vocals");
    Track* track = manager.trackListModel()->getTrackById(trackId);
    track->setVolume(0.0f); // Start silent

    // Add a clip on the track
    manager.addClipToTrack(trackId, "clip_test", Clip::Audio, 0LL, 1000000LL, "mock_audio.wav");

    // Load mock samples into the cache
    AudioReader* reader = new AudioReader();
    std::vector<float> mockSamples(96000, 1.0f); // 1.0f DC signal
    reader->setSamplesForTesting(mockSamples);
    engine.setCachedReaderForTesting("mock_audio.wav", reader);

    // Start playback
    engine.play();

    // 1. Initial mix pass: track volume is 0.0f, so mixed output should be completely silent
    std::vector<float> outputBuffer(512 * 2, 999.0f); // Fill with dummy values
    engine.mixAudio(outputBuffer.data(), 512);

    for (int i = 0; i < 512 * 2; ++i) {
        assert(std::abs(outputBuffer[i]) < 0.0001f);
    }

    // 2. Change volume to 1.0f (full volume)
    track->setVolume(1.0f);

    // 3. Next mix pass: we mix 512 frames.
    // The volume ramp should run over the first 256 samples, scaling from 0.0f to 1.0f.
    // The remaining 256 samples should be mixed at exactly 1.0f volume (mixed value should be exactly 1.0f * 1.0f = 1.0f).
    engine.mixAudio(outputBuffer.data(), 512);

    // Check ramp: first 256 samples should increase progressively
    for (int i = 0; i < 256; ++i) {
        float expectedGain = static_cast<float>(i) / 256.0f;
        assert(std::abs(outputBuffer[i * 2] - expectedGain) < 0.001f);
        assert(std::abs(outputBuffer[i * 2 + 1] - expectedGain) < 0.001f);
    }

    // Check post-ramp: remaining 256 samples should be exactly 1.0f
    for (int i = 256; i < 512; ++i) {
        assert(std::abs(outputBuffer[i * 2] - 1.0f) < 0.001f);
        assert(std::abs(outputBuffer[i * 2 + 1] - 1.0f) < 0.001f);
    }

    engine.stop();
    std::cout << "[PASS] Audio Gain Ramping test completed successfully.\n\n";
}

void testResampler() {
    std::cout << "[TEST] Running StemResampler test...\n";

    // Generate a 1-second stereo sine wave at 48000Hz (48000 frames * 2 channels = 96000 samples)
    std::vector<float> input(96000);
    for (size_t i = 0; i < 48000; ++i) {
        float val = std::sin(2.0f * 3.14159265f * 440.0f * i / 48000.0f);
        input[i * 2] = val;     // Left
        input[i * 2 + 1] = val; // Right
    }

    // Downsample to 44100Hz
    std::vector<float> output = StemResampler::resample(input, 48000, 44100, 2);
    assert(!output.empty());
    
    // The output sample count should be approximately 44100 * 2 = 88200 samples.
    // Allow small window tolerance for resampling filter delays.
    size_t expectedSize = 44100 * 2;
    std::cout << "[INFO] Resampler output size: " << output.size() << " (Expected approx: " << expectedSize << ")\n";
    assert(std::abs(static_cast<long long>(output.size()) - static_cast<long long>(expectedSize)) < 100);

    // Upsample back to 48000Hz
    std::vector<float> recovered = StemResampler::resample(output, 44100, 48000, 2);
    assert(!recovered.empty());
    size_t expectedRecoveredSize = 48000 * 2;
    std::cout << "[INFO] Resampler recovered size: " << recovered.size() << " (Expected approx: " << expectedRecoveredSize << ")\n";
    assert(std::abs(static_cast<long long>(recovered.size()) - static_cast<long long>(expectedRecoveredSize)) < 100);

    std::cout << "[PASS] StemResampler test completed successfully.\n\n";
}

void testOverlapAddWindowing() {
    std::cout << "[TEST] Running Overlap-Add Windowing Math test...\n";

    // Set up sliding window parameters identical to StemSeparator
    const int64_t W = 441000; // 10 seconds window
    const int64_t O = 44100;  // 1 second overlap
    const int64_t S = W - O;  // Step size

    // Let's simulate a small timeline of 3 windows
    // N = 2 * S + W = 2 * (W - O) + W = 3 * W - 2 * O
    const size_t N = 3 * W - 2 * O;
    std::vector<float> windowWeightsAccumulated(N, 0.0f);

    // Slide across audio in steps of S
    for (size_t offset = 0; offset < N; offset += S) {
        size_t chunk_len = std::min<size_t>(W, N - offset);
        if (chunk_len == 0) break;

        for (size_t t = 0; t < chunk_len; ++t) {
            float weight = 1.0f;

            // Left overlap region (ramp up)
            if (offset > 0 && t < O) {
                float theta = (3.1415926535f * t) / (2.0f * O);
                weight = std::sin(theta) * std::sin(theta);
            }
            // Right overlap region (ramp down)
            else if (offset + W < N && t >= W - O) {
                size_t t_prime = t - (W - O);
                float theta = (3.1415926535f * t_prime) / (2.0f * O);
                weight = std::cos(theta) * std::cos(theta);
            }

            size_t global_idx = offset + t;
            if (global_idx < N) {
                windowWeightsAccumulated[global_idx] += weight;
            }
        }
        if (offset + W >= N) {
            break;
        }
    }

    // Now assert that for ALL indices on the timeline, the weight is exactly 1.0f (or extremely close due to floating point precision)
    for (size_t i = 0; i < N; ++i) {
        float w = windowWeightsAccumulated[i];
        // Allow a tiny tolerance for floating-point calculation
        assert(std::abs(w - 1.0f) < 1e-5f);
    }

    std::cout << "[PASS] Overlap-Add Windowing Math test completed successfully.\n\n";
}

void testStemSeparatorFallback() {
    std::cout << "[TEST] Running StemSeparator Fallback test...\n";

    StemSeparator separator;
    // Attempting to load from a non-existent path must return false
    bool success = separator.initialize("C:/non_existent_path_to_model/non_existent.onnx");
    assert(!success);
    assert(!separator.isInitialized());
    assert(separator.modelName().isEmpty());
    // Fallback EP should remain CPU
    assert(separator.executionProvider() == "CPU");

    std::cout << "[PASS] StemSeparator Fallback test completed successfully.\n\n";
}

void testOfflineMixdown() {
    std::cout << "[TEST] Running Offline Mixdown test...\n";

    TimelineManager manager;
    AudioEngine engine(&manager);

    // Add audio track
    QString trackId = manager.addTrack(Track::Audio, "Background Vocals");
    Track* track = manager.trackListModel()->getTrackById(trackId);
    track->setVolume(0.8f);

    // Add a clip on the track
    manager.addClipToTrack(trackId, "clip_test_offline", Clip::Audio, 0LL, 1000000LL, "mock_offline_audio.wav");

    // Load mock samples into the cache (1.0f constant DC signal)
    AudioReader* reader = new AudioReader();
    std::vector<float> mockSamples(96000, 1.0f);
    reader->setSamplesForTesting(mockSamples);
    engine.setCachedReaderForTesting("mock_offline_audio.wav", reader);

    // Run mixOffline for 1024 samples starting at sample index 0
    std::vector<float> outputBuffer(1024 * 2, 0.0f);
    engine.mixOffline(outputBuffer.data(), 1024, 0);

    // Verify mixed output matches mock audio amplitude scaled by track volume (1.0f * 0.8f = 0.8f)
    for (int i = 0; i < 1024 * 2; ++i) {
        if (std::abs(outputBuffer[i] - 0.8f) >= 0.0001f) {
            std::cout << "[ERROR] outputBuffer[" << i << "] = " << outputBuffer[i] << " (expected 0.8)\n";
            assert(false);
        }
    }

    std::cout << "[PASS] Offline Mixdown test completed successfully.\n\n";
}

void testRenderEngineInitialization() {
    std::cout << "[TEST] Running Render Engine Initialization test...\n";

    // 1. Test MP4 container with H.264 / AAC
    {
        QString tempRenderPath = QDir::tempPath() + "/test_render_output.mp4";
        std::cout << "[INFO] Initializing test render (MP4) to: " << tempRenderPath.toStdString() << "\n";

        RenderEngine engine;
        bool success = engine.startRender(tempRenderPath, 640, 360, 30, 2000000, 128000);
        assert(success);
        
        QString videoCodec = engine.chosenVideoCodec();
        QString audioCodec = engine.chosenAudioCodec();
        std::cout << "[INFO] Chosen Video Codec (MP4): " << videoCodec.toStdString() << "\n";
        std::cout << "[INFO] Chosen Audio Codec (MP4): " << audioCodec.toStdString() << "\n";
        assert(videoCodec.contains("h264") || videoCodec.contains("libx264") || videoCodec.contains("nvenc") || videoCodec.contains("amf") || videoCodec.contains("qsv"));
        assert(audioCodec.contains("aac"));

        QImage dummyFrame(640, 360, QImage::Format_RGBA8888);
        dummyFrame.fill(Qt::blue);
        for (int i = 0; i < 10; ++i) {
            bool written = engine.writeVideoFrame(dummyFrame, i);
            assert(written);
        }

        std::vector<float> silence(1600 * 2, 0.0f);
        for (int i = 0; i < 10; ++i) {
            bool written = engine.writeAudioFrame(silence.data(), 1600);
            assert(written);
        }

        bool finished = engine.finishRender();
        assert(finished);

        QFile file(tempRenderPath);
        assert(file.exists());
        assert(file.size() > 100);
        file.remove();
    }

    // 2. Test AVI container with MPEG-4 / MP3
    {
        QString tempRenderPath = QDir::tempPath() + "/test_render_output.avi";
        std::cout << "[INFO] Initializing test render (AVI) to: " << tempRenderPath.toStdString() << "\n";

        RenderEngine engine;
        bool success = engine.startRender(tempRenderPath, 640, 360, 30, 2000000, 128000);
        assert(success);
        
        QString videoCodec = engine.chosenVideoCodec();
        QString audioCodec = engine.chosenAudioCodec();
        std::cout << "[INFO] Chosen Video Codec (AVI): " << videoCodec.toStdString() << "\n";
        std::cout << "[INFO] Chosen Audio Codec (AVI): " << audioCodec.toStdString() << "\n";
        assert(videoCodec.contains("mpeg4"));
        assert(audioCodec.contains("mp3") || audioCodec.contains("libmp3lame"));

        QImage dummyFrame(640, 360, QImage::Format_RGBA8888);
        dummyFrame.fill(Qt::red);
        for (int i = 0; i < 10; ++i) {
            bool written = engine.writeVideoFrame(dummyFrame, i);
            assert(written);
        }

        std::vector<float> silence(1600 * 2, 0.0f);
        for (int i = 0; i < 10; ++i) {
            bool written = engine.writeAudioFrame(silence.data(), 1600);
            assert(written);
        }

        bool finished = engine.finishRender();
        assert(finished);

        QFile file(tempRenderPath);
        assert(file.exists());
        assert(file.size() > 100);
        file.remove();
    }

    // 3. Test MKV container with H.264 / FLAC (custom override)
    {
        QString tempRenderPath = QDir::tempPath() + "/test_render_output.mkv";
        std::cout << "[INFO] Initializing test render (MKV FLAC) to: " << tempRenderPath.toStdString() << "\n";

        RenderEngine engine;
        bool success = engine.startRender(tempRenderPath, 640, 360, 30, 2000000, 128000, "FLAC");
        assert(success);
        
        QString videoCodec = engine.chosenVideoCodec();
        QString audioCodec = engine.chosenAudioCodec();
        std::cout << "[INFO] Chosen Video Codec (MKV FLAC): " << videoCodec.toStdString() << "\n";
        std::cout << "[INFO] Chosen Audio Codec (MKV FLAC): " << audioCodec.toStdString() << "\n";
        assert(videoCodec.contains("h264") || videoCodec.contains("libx264") || videoCodec.contains("nvenc") || videoCodec.contains("amf") || videoCodec.contains("qsv"));
        assert(audioCodec.contains("flac"));

        QImage dummyFrame(640, 360, QImage::Format_RGBA8888);
        dummyFrame.fill(Qt::green);
        for (int i = 0; i < 10; ++i) {
            bool written = engine.writeVideoFrame(dummyFrame, i);
            assert(written);
        }

        std::vector<float> silence(1600 * 2, 0.0f);
        for (int i = 0; i < 10; ++i) {
            bool written = engine.writeAudioFrame(silence.data(), 1600);
            assert(written);
        }

        bool finished = engine.finishRender();
        assert(finished);

        QFile file(tempRenderPath);
        assert(file.exists());
        assert(file.size() > 100);
        file.remove();
    }

    std::cout << "[PASS] Render Engine Initialization test completed successfully.\n\n";
}

void testMarkersAndSubtitleStyles() {
    std::cout << "[TEST] Running Markers & Subtitle Styles test...\n";

    TimelineManager manager;

    // Test default subtitle styles
    assert(manager.subtitleFontFamily() == "Outfit");
    assert(manager.subtitleFontSize() == 24);
    assert(manager.subtitleFillColor() == "#4A4A5A");
    assert(manager.subtitleActiveColor() == "#00E676");
    assert(manager.subtitleOutlineColor() == "#08080A");
    assert(manager.subtitleOutlineWidth() == 2);

    // Modify subtitle styles
    manager.setSubtitleFontFamily("Inter");
    manager.setSubtitleFontSize(32);
    manager.setSubtitleFillColor("#FFFFFF");
    manager.setSubtitleActiveColor("#FF4081");
    manager.setSubtitleOutlineColor("#2C2C35");
    manager.setSubtitleOutlineWidth(5);

    assert(manager.subtitleFontFamily() == "Inter");
    assert(manager.subtitleFontSize() == 32);
    assert(manager.subtitleFillColor() == "#FFFFFF");
    assert(manager.subtitleActiveColor() == "#FF4081");
    assert(manager.subtitleOutlineColor() == "#2C2C35");
    assert(manager.subtitleOutlineWidth() == 5);

    // Test sequence markers
    assert(manager.markers().isEmpty());

    manager.addMarker(10000000LL, "Intro Chorus", "blue");
    manager.addMarker(5000000LL, "First Vocal", "green");
    manager.addMarker(20000000LL, "Outro", "yellow");

    QVariantList markers = manager.markers();
    assert(markers.size() == 3);

    // They should be sorted by timeUs ascending: 5s, 10s, 20s
    assert(markers[0].toMap()["timeUs"].toLongLong() == 5000000LL);
    assert(markers[0].toMap()["name"].toString() == "First Vocal");
    assert(markers[0].toMap()["color"].toString() == "green");

    assert(markers[1].toMap()["timeUs"].toLongLong() == 10000000LL);
    assert(markers[1].toMap()["name"].toString() == "Intro Chorus");
    assert(markers[1].toMap()["color"].toString() == "blue");

    assert(markers[2].toMap()["timeUs"].toLongLong() == 20000000LL);
    assert(markers[2].toMap()["name"].toString() == "Outro");
    assert(markers[2].toMap()["color"].toString() == "yellow");

    QString markerId1 = markers[0].toMap()["id"].toString();
    QString markerId2 = markers[1].toMap()["id"].toString();

    // Update marker
    manager.updateMarker(markerId1, "Vocal Start", "red");
    QVariantList updated = manager.markers();
    assert(updated[0].toMap()["name"].toString() == "Vocal Start");
    assert(updated[0].toMap()["color"].toString() == "red");

    // Remove marker
    manager.removeMarker(markerId2);
    assert(manager.markers().size() == 2);
    assert(manager.markers()[0].toMap()["name"].toString() == "Vocal Start");
    assert(manager.markers()[1].toMap()["name"].toString() == "Outro");

    // Test project save/load serialization of styles and markers
    QString savePath = QDir::tempPath() + "/test_markers_project.nctv";
    bool saved = manager.saveProject(savePath);
    assert(saved);

    manager.clearProject();
    assert(manager.markers().isEmpty());
    assert(manager.subtitleFontFamily() == "Outfit"); // Reset to defaults on clear

    bool loaded = manager.loadProject(savePath);
    assert(loaded);

    // Verify properties loaded successfully
    assert(manager.subtitleFontFamily() == "Inter");
    assert(manager.subtitleFontSize() == 32);
    assert(manager.subtitleFillColor() == "#FFFFFF");
    assert(manager.subtitleActiveColor() == "#FF4081");
    assert(manager.subtitleOutlineColor() == "#2C2C35");
    assert(manager.subtitleOutlineWidth() == 5);

    assert(manager.markers().size() == 2);
    assert(manager.markers()[0].toMap()["name"].toString() == "Vocal Start");
    assert(manager.markers()[0].toMap()["color"].toString() == "red");
    assert(manager.markers()[1].toMap()["name"].toString() == "Outro");
    assert(manager.markers()[1].toMap()["color"].toString() == "yellow");

    QFile::remove(savePath);
    std::cout << "[PASS] Markers & Subtitle Styles test completed successfully.\n\n";
}

void testRomanization() {
    std::cout << "[TEST] Running Romanization tests...\n";
    
    TimelineManager manager;
    
    // 1. Korean
    QString kr1 = "안녕하세요";
    QString romKr1 = manager.romanizeText(kr1);
    assert(romKr1 == "annyeonghaseyo");

    QString kr2 = "사랑해";
    QString romKr2 = manager.romanizeText(kr2);
    assert(romKr2 == "saranghae");

    // 2. Japanese Hiragana
    QString jp1 = "ありがとう";
    QString romJp1 = manager.romanizeText(jp1);
    assert(romJp1 == "arigatou");

    // Japanese Katakana
    QString jp2 = "サクラ";
    QString romJp2 = manager.romanizeText(jp2);
    assert(romJp2 == "sakura");

    // Japanese double consonant small tsu
    QString jp3 = "がっこう";
    QString romJp3 = manager.romanizeText(jp3);
    assert(romJp3 == "gakkou");

    // Japanese blend
    QString jp4 = "きょう";
    QString romJp4 = manager.romanizeText(jp4);
    assert(romJp4 == "kyou");
    
    // Japanese shya -> sha blend
    QString jp5 = "いっしゃ";
    QString romJp5 = manager.romanizeText(jp5);
    assert(romJp5 == "issha");

    // Mixed text
    QString mixed = "Hello 안녕さくら!";
    QString romMixed = manager.romanizeText(mixed);
    assert(romMixed == "Hello annyeongsakura!");

    // 3. Clip Romanize Verification
    // Create a track and a clip
    QString trackId = manager.addTrack(Track::Lyrics, "Lyrics Track");
    bool added = manager.addClipToTrack(trackId, "test_clip", 2, 1000000LL, 5000000LL, "", "안녕");
    assert(added);
    
    Track* track = manager.trackListModel()->getTrackById(trackId);
    assert(track != nullptr);
    Clip* clip = track->clips().first();
    assert(clip != nullptr);
    
    // Verify initial state
    assert(clip->lyricText() == "안녕");
    assert(clip->syllables().size() == 1);
    assert(clip->syllables()[0].toMap()["text"].toString().trimmed() == "안녕");

    // Romanize clip
    manager.romanizeClip(clip);

    // Verify after state
    assert(clip->lyricText() == "annyeong");
    assert(clip->syllables().size() == 1);
    assert(clip->syllables()[0].toMap()["text"].toString().trimmed() == "annyeong");

    std::cout << "[PASS] Romanization tests completed successfully.\n\n";
}

void testLyricsStringImport() {
    std::cout << "[TEST] Running Lyrics String Import test...\n";

    TimelineManager manager;
    QString trackId = manager.addTrack(Track::Lyrics, "Lyrics Track");
    Track* track = manager.trackListModel()->getTrackById(trackId);
    assert(track != nullptr);

    // Test LRC timed lyrics string
    QString lrcContent = 
        "[00:02.50]Line One Text\n"
        "[00:06.00]Line Two Text\n";

    bool success = manager.importLyricsFromString(trackId, lrcContent);
    assert(success);

    // Verify clips were successfully added
    const QList<Clip*>& clips = track->clips();
    assert(clips.size() == 2);

    assert(clips[0]->startTime() == 2500000LL); // 2.5s in microseconds
    assert(clips[0]->lyricText() == "Line One Text");
    assert(clips[0]->duration() == 3500000LL); // 6.0s - 2.5s = 3.5s

    assert(clips[1]->startTime() == 6000000LL); // 6.0s in microseconds
    assert(clips[1]->lyricText() == "Line Two Text");
    assert(clips[1]->duration() == 4000000LL); // Default 4.0s duration for last clip

    std::cout << "[PASS] Lyrics String Import test completed successfully.\n\n";
}

void testLyricEngine() {
    std::cout << "[TEST] Running LyricEngine test...\n";

    TimelineManager manager;
    LyricEngine* engine = manager.lyricEngine();
    assert(engine != nullptr);

    // 1. Initial State
    assert(engine->displayMode() == LyricEngine::BottomTwoLine);
    assert(!engine->hasActiveLine());
    assert(engine->activeLineText().isEmpty());

    // 2. Add some lyrics clips with syllables
    QString trackId = manager.addTrack(Track::Lyrics, "Lyrics Track");
    
    // Add clip 1: 2.0s -> 5.0s (duration 3.0s)
    bool added1 = manager.addClipToTrack(trackId, "lyr_c1", Clip::Lyrics, 2000000LL, 3000000LL, "", "Hello World");
    assert(added1);
    
    Track* track = manager.trackListModel()->getTrackById(trackId);
    Clip* clip1 = track->clips().first();
    // Setup 2 words: "Hello" (duration 1.5s), "World" (duration 1.5s)
    // Relative start times: 0.0s, 1.5s
    QVariantList syllables1;
    QVariantMap syl1_1, syl1_2;
    syl1_1["text"] = "Hello";
    syl1_1["relativeStart"] = 0LL;
    syl1_1["duration"] = 1500000LL;
    syl1_2["text"] = "World";
    syl1_2["relativeStart"] = 1500000LL;
    syl1_2["duration"] = 1500000LL;
    syllables1.append(syl1_1);
    syllables1.append(syl1_2);
    clip1->setSyllables(syllables1);

    // Add clip 2: 6.0s -> 9.0s (duration 3.0s)
    bool added2 = manager.addClipToTrack(trackId, "lyr_c2", Clip::Lyrics, 6000000LL, 3000000LL, "", "NC-KTV V2");
    assert(added2);

    // Rebuild cache (TimelineManager track mutations do this automatically, but let's be safe)
    engine->rebuildLineCache();

    // 3. Test active line at 3.0s (relative playback time 1.0s in clip 1)
    engine->updatePlaybackPosition(3000000LL);
    assert(engine->hasActiveLine());
    assert(engine->activeLineText() == "Hello World");
    assert(engine->nextLineText() == "NC-KTV V2");
    assert(engine->activeLineStartTime() == 2000000LL);
    assert(engine->activeLineEndTime() == 5000000LL);
    assert(engine->nextLineStartTime() == 6000000LL);

    // Verify word tracking: at 3.0s (1.0s elapsed in 3.0s clip)
    // "Hello" is active (starts at 0.0s, duration 1.5s)
    assert(engine->activeWordIndex() == 0);
    assert(std::abs(engine->activeWordProgress() - (1000000.0 / 1500000.0)) < 0.001);

    // 4. Test active line at 4.0s (relative playback time 2.0s in clip 1)
    engine->updatePlaybackPosition(4000000LL);
    // "World" is active (starts at 1.5s, duration 1.5s)
    assert(engine->activeWordIndex() == 1);
    assert(std::abs(engine->activeWordProgress() - (500000.0 / 1500000.0)) < 0.001);

    // 5. Test active line between clips (e.g. at 5.5s)
    engine->updatePlaybackPosition(5500000LL);
    assert(!engine->hasActiveLine());
    assert(engine->nextLineText() == "NC-KTV V2");
    assert(engine->nextLineStartTime() == 6000000LL);

    // 6. Test mode setting
    engine->setDisplayMode(LyricEngine::CinematicFullScreen);
    assert(engine->displayMode() == LyricEngine::CinematicFullScreen);

    std::cout << "[PASS] LyricEngine test completed successfully.\n\n";
}

void testMetadataGuesser() {
    std::cout << "[TEST] Running Metadata Guesser (Filename Parser) test...\n";

    TimelineManager manager;

    // We can trigger it by adding a clip to a track
    QString trackId = manager.addTrack(Track::Audio, "Background Audio");

    // Test 1: Standard "Artist - Title" format with noise suffix
    manager.addClipToTrack(trackId, "clip_guess_1", Track::Audio, 0LL, 1000000LL, "queen - bohemian rhapsody (official video).wav");
    assert(manager.artistName() == "Queen");
    assert(manager.songTitle() == "Bohemian Rhapsody");

    // Reset metadata for next test
    manager.setArtistName("Unknown Artist");
    manager.setSongTitle("Untitled Song");

    // Test 2: Another delimiter " - " and lowercase capitalization
    manager.addClipToTrack(trackId, "clip_guess_2", Track::Audio, 1000000LL, 1000000LL, "michael jackson - billie jean_vocals.mp3");
    assert(manager.artistName() == "Michael Jackson");
    assert(manager.songTitle() == "Billie Jean");

    std::cout << "[PASS] Metadata Guesser test completed successfully.\n\n";
}

void testHwDecodingPreferences() {
    std::cout << "[TEST] Running HW Decoding Preferences test...\n";

    TimelineManager manager;
    // Store initial state
    bool originalVal = manager.disableHwDecoding();

    // Toggle setting
    manager.setDisableHwDecoding(true);
    assert(manager.disableHwDecoding() == true);

    manager.setDisableHwDecoding(false);
    assert(manager.disableHwDecoding() == false);

    // Restore original val
    manager.setDisableHwDecoding(originalVal);

    std::cout << "[PASS] HW Decoding Preferences test completed successfully.\n\n";
}

void testTimelineChangedPropagation() {
    std::cout << "[TEST] Running timelineChanged Signal Propagation test...\n";

    TimelineManager manager;
    bool signalEmitted = false;
    QObject::connect(&manager, &TimelineManager::timelineChanged, [&]() {
        signalEmitted = true;
    });

    // 1. Trigger via addTrack
    QString trackId = manager.addTrack(Track::Audio, "Test Track");
    assert(signalEmitted);
    signalEmitted = false; // Reset

    // 2. Trigger via addClipToTrack
    bool clipAdded = manager.addClipToTrack(trackId, "clip_sig_test", Track::Audio, 0LL, 1000000LL, "test_file.wav");
    assert(clipAdded);
    assert(signalEmitted);
    signalEmitted = false; // Reset

    // 3. Trigger via removeTrack
    bool trackRemoved = manager.removeTrack(trackId);
    assert(trackRemoved);
    assert(signalEmitted);

    std::cout << "[PASS] timelineChanged Signal Propagation test completed successfully.\n\n";
}

void testMidSideDspSeparation() {
    std::cout << "[TEST] Running Mid-Side DSP Separation (Fallback) test...\n";

    // Create synthetic stereo interleaved audio: L=0.8, R=0.2 for 1000 stereo frames
    const size_t numFrames = 1000;
    const size_t totalSamples = numFrames * 2; // stereo interleaved
    std::vector<float> input(totalSamples);
    for (size_t i = 0; i < numFrames; ++i) {
        input[i * 2]     = 0.8f;  // Left
        input[i * 2 + 1] = 0.2f;  // Right
    }

    // Run Mid-Side separation (same algorithm as StemSeparationWorker fallback)
    std::vector<float> vocals(totalSamples, 0.0f);
    std::vector<float> instrumental(totalSamples, 0.0f);

    for (size_t idx = 0; idx < numFrames; ++idx) {
        size_t i = idx * 2;
        float L = input[i];
        float R = input[i + 1];

        // Mid channel: center signals (vocals)
        float mid = (L + R) * 0.5f;
        vocals[i]     = mid;
        vocals[i + 1] = mid;

        // Side channel: stereo difference (instrumentals)
        float sideL = (L - R) * 0.5f;
        float sideR = (R - L) * 0.5f;
        instrumental[i]     = sideL;
        instrumental[i + 1] = sideR;
    }

    // Verify vocal samples: Mid = (0.8 + 0.2) / 2 = 0.5 for both channels
    for (size_t i = 0; i < totalSamples; ++i) {
        assert(std::abs(vocals[i] - 0.5f) < 1e-6f);
    }

    // Verify instrumental samples:
    // Side L = (0.8 - 0.2) / 2 = 0.3
    // Side R = (0.2 - 0.8) / 2 = -0.3
    for (size_t idx = 0; idx < numFrames; ++idx) {
        size_t i = idx * 2;
        assert(std::abs(instrumental[i] - 0.3f) < 1e-6f);
        assert(std::abs(instrumental[i + 1] - (-0.3f)) < 1e-6f);
    }

    // Verify WAV file writing via StemSeparator helper
    StemSeparator separator;
    QString vocalsPath = QDir::tempPath() + "/test_midside_vocals.wav";
    QString instPath = QDir::tempPath() + "/test_midside_inst.wav";

    bool vocalsWritten = separator.writeWavFile(vocalsPath, vocals, 48000);
    bool instWritten = separator.writeWavFile(instPath, instrumental, 48000);
    assert(vocalsWritten);
    assert(instWritten);

    // Verify files exist and are non-empty
    QFile vFile(vocalsPath);
    assert(vFile.exists());
    assert(vFile.size() > 44); // WAV header is 44 bytes minimum

    QFile iFile(instPath);
    assert(iFile.exists());
    assert(iFile.size() > 44);

    // Cleanup
    QFile::remove(vocalsPath);
    QFile::remove(instPath);

    // Also verify the existing StemSeparator initialization failure path
    // (ONNX model not found -> should fail gracefully, EP stays CPU)
    bool dummyInit = separator.initialize("C:/non_existent_path_to_model/non_existent.onnx");
    assert(!dummyInit);
    assert(!separator.isInitialized());
    assert(separator.executionProvider() == "CPU");

    std::cout << "[PASS] Mid-Side DSP Separation (Fallback) test completed successfully.\n\n";
}

void testWaveformCaching() {
    std::cout << "[TEST] Running Waveform Caching test...\n";

    QString tempFilePath = QDir::tempPath() + "/mock_source_audio.wav";
    QFile tempFile(tempFilePath);
    if (tempFile.open(QIODevice::WriteOnly)) {
        tempFile.write("RIFFxxxxWAVEfmt "); // dummy bytes
        tempFile.close();
    }
    assert(QFile::exists(tempFilePath));

    AudioReader reader;
    const size_t totalSamples = 48000 * 2 * 2; // 2 seconds stereo at 48000 Hz
    std::vector<float> mockSamples(totalSamples, 0.0f);
    for (size_t i = 0; i < totalSamples; i += 2) {
        mockSamples[i] = 0.5f * std::sin(2.0 * 3.141592653589793 * 440.0 * (i / 2.0) / 48000.0);
        mockSamples[i + 1] = -0.3f * std::cos(2.0 * 3.141592653589793 * 440.0 * (i / 2.0) / 48000.0);
    }
    
    reader.setSamplesForTesting(mockSamples, 48000);

    assert(!reader.peaks256().empty());
    assert(!reader.peaks4096().empty());
    assert(reader.sampleRate() == 48000);
    assert(reader.channels() == 2);
    assert(std::abs(reader.durationSeconds() - 2.0) < 1e-6);

    bool saved = reader.savePeakCache(tempFilePath);
    assert(saved);

    QString cachePath = reader.getCachePath(tempFilePath);
    assert(QFile::exists(cachePath));

    AudioReader loadedReader;
    bool loaded = loadedReader.loadPeakCache(tempFilePath);
    assert(loaded);

    assert(loadedReader.sampleRate() == reader.sampleRate());
    assert(loadedReader.channels() == reader.channels());
    assert(std::abs(loadedReader.durationSeconds() - reader.durationSeconds()) < 1e-6);
    assert(loadedReader.peaks256().size() == reader.peaks256().size());
    assert(loadedReader.peaks4096().size() == reader.peaks4096().size());

    for (size_t i = 0; i < reader.peaks256().size(); ++i) {
        assert(std::abs(loadedReader.peaks256()[i].minVal - reader.peaks256()[i].minVal) < 1e-6f);
        assert(std::abs(loadedReader.peaks256()[i].maxVal - reader.peaks256()[i].maxVal) < 1e-6f);
    }

    QFile::remove(tempFilePath);
    QFile::remove(cachePath);

    std::cout << "[PASS] Waveform Caching test completed successfully.\n\n";
}

double solveBezierLocal(double x, double x1, double y1, double x2, double y2) {
    if (x <= 0.0) return 0.0;
    if (x >= 1.0) return 1.0;

    x1 = std::max(0.0, std::min(1.0, x1));
    x2 = std::max(0.0, std::min(1.0, x2));

    double t = x; 
    double A = 3.0 * x1 - 3.0 * x2 + 1.0;
    double B = 3.0 * x2 - 6.0 * x1;
    double C = 3.0 * x1;

    for (int i = 0; i < 8; ++i) {
        double xVal = ((A * t + B) * t + C) * t;
        double dx = (3.0 * A * t + 2.0 * B) * t + C;
        if (std::abs(dx) < 1e-6) break;
        double diff = xVal - x;
        t -= diff / dx;
        t = std::max(0.0, std::min(1.0, t));
    }

    double finalX = ((A * t + B) * t + C) * t;
    if (std::abs(finalX - x) > 1e-3) {
        double lo = 0.0;
        double hi = 1.0;
        t = x;
        for (int i = 0; i < 16; ++i) {
            double xVal = ((A * t + B) * t + C) * t;
            if (std::abs(xVal - x) < 1e-4) break;
            if (xVal < x) {
                lo = t;
            } else {
                hi = t;
            }
            t = (lo + hi) / 2.0;
        }
    }

    double Ay = 3.0 * y1 - 3.0 * y2 + 1.0;
    double By = 3.0 * y2 - 6.0 * y1;
    double Cy = 3.0 * y1;
    return ((Ay * t + By) * t + Cy) * t;
}

void testBezierTimingCurves() {
    std::cout << "[TEST] Running Bezier Timing Curves test...\n";

    double out1 = solveBezierLocal(0.5, 0.25, 0.25, 0.75, 0.75);
    assert(std::abs(out1 - 0.5) < 1e-4);

    double out2 = solveBezierLocal(0.0, 0.42, 0.0, 0.58, 1.0);
    assert(std::abs(out2 - 0.0) < 1e-4);
    double out3 = solveBezierLocal(1.0, 0.42, 0.0, 0.58, 1.0);
    assert(std::abs(out3 - 1.0) < 1e-4);
    
    double outMid = solveBezierLocal(0.5, 0.42, 0.0, 0.58, 1.0);
    assert(std::abs(outMid - 0.5) < 1e-4);

    Clip clip("test_clip_bezier", Clip::Lyrics, 0, 10000000); 
    clip.setLyricText("Hello world");
    
    QVariantList syllables = clip.syllables();
    assert(!syllables.isEmpty());
    for (const QVariant& s : syllables) {
        QVariantMap map = s.toMap();
        assert(map["x1"].toDouble() == 0.25);
        assert(map["y1"].toDouble() == 0.25);
        assert(map["x2"].toDouble() == 0.75);
        assert(map["y2"].toDouble() == 0.75);
    }

    clip.updateSyllableCurve(0, 0.1, 0.2, 0.3, 0.4);
    QVariantMap updatedMap = clip.syllables()[0].toMap();
    assert(updatedMap["x1"].toDouble() == 0.1);
    assert(updatedMap["y1"].toDouble() == 0.2);
    assert(updatedMap["x2"].toDouble() == 0.3);
    assert(updatedMap["y2"].toDouble() == 0.4);

    nlohmann::json clipJson = clip.toJson();
    Clip* loadedClip = Clip::fromJson(clipJson);
    assert(loadedClip != nullptr);
    assert(loadedClip->clipId() == "test_clip_bezier");
    
    QVariantList loadedSyllables = loadedClip->syllables();
    assert(loadedSyllables.size() == syllables.size());
    QVariantMap loadedFirstSyl = loadedSyllables[0].toMap();
    assert(loadedFirstSyl["x1"].toDouble() == 0.1);
    assert(loadedFirstSyl["y1"].toDouble() == 0.2);
    assert(loadedFirstSyl["x2"].toDouble() == 0.3);
    assert(loadedFirstSyl["y2"].toDouble() == 0.4);

    delete loadedClip;

    std::cout << "[PASS] Bezier Timing Curves test completed successfully.\n\n";
}

void testCrossCorrelationAlignment() {
    std::cout << "[TEST] Running Vocal Alignment Cross-Correlation test...\n";

    QString guidePath = QDir::tempPath() + "/mock_guide.wav";
    QString targetPath = QDir::tempPath() + "/mock_target.wav";

    const int sampleRate = 48000;
    const size_t numFrames = sampleRate * 2;
    const size_t totalSamples = numFrames * 2;
    
    std::vector<float> guideSamples(totalSamples, 0.0f);
    std::vector<float> targetSamples(totalSamples, 0.0f);

    double stddev = 0.1; 
    for (size_t i = 0; i < numFrames; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        float valGuide = static_cast<float>(std::exp(-0.5 * std::pow((t - 1.0) / stddev, 2)));
        float valTarget = static_cast<float>(std::exp(-0.5 * std::pow((t - 0.8) / stddev, 2)));

        guideSamples[i * 2] = valGuide;
        guideSamples[i * 2 + 1] = valGuide;

        targetSamples[i * 2] = valTarget;
        targetSamples[i * 2 + 1] = valTarget;
    }

    StemSeparator separator;
    bool guideWritten = separator.writeWavFile(guidePath, guideSamples, sampleRate);
    bool targetWritten = separator.writeWavFile(targetPath, targetSamples, sampleRate);
    assert(guideWritten);
    assert(targetWritten);

    TimelineManager manager;
    QString guideTrackId = manager.addTrack(Track::Audio, "Guide Track");
    QString targetTrackId = manager.addTrack(Track::Audio, "Target Track");

    bool c1 = manager.addClipToTrack(guideTrackId, "guide_clip", Track::Audio, 0, 2000000, guidePath);
    bool c2 = manager.addClipToTrack(targetTrackId, "target_clip", Track::Audio, 1000000, 2000000, targetPath);
    assert(c1);
    assert(c2);

    double offset = manager.alignAudioClip("target_clip", "guide_clip");
    assert(std::abs(offset - (-0.8)) < 0.05);

    Clip* targetClip = nullptr;
    for (Track* t : manager.trackListModel()->tracks()) {
        if (Clip* c = t->getClip("target_clip")) {
            targetClip = c;
            break;
        }
    }
    assert(targetClip != nullptr);
    assert(std::abs(targetClip->startTime() - 200000LL) < 50000LL); 

    QFile::remove(guidePath);
    QFile::remove(targetPath);

    std::cout << "[PASS] Vocal Alignment Cross-Correlation test completed successfully.\n\n";
}

void testAudioPlaybackLoop() {
    std::cout << "[TEST] Running Audio Playback Loop test...\n";

    TimelineManager manager;
    AudioEngine engine(&manager);

    // Setup audio track & clip so mixAudio can run without crashing
    QString trackId = manager.addTrack(Track::Audio, "TestTrack");
    manager.addClipToTrack(trackId, "clip_test_loop", Clip::Audio, 0LL, 5000000LL, "mock_loop.wav");

    AudioReader* reader = new AudioReader();
    std::vector<float> mockSamples(480000, 1.0f); // 10 seconds of mock audio
    reader->setSamplesForTesting(mockSamples);
    engine.setCachedReaderForTesting("mock_loop.wav", reader);

    // Setup loop listener on currentPlayheadTimeChanged (similar to QML)
    qint64 loopStartUs = 1000000LL; // 1.0s
    qint64 loopEndUs = 2000000LL;   // 2.0s
    bool loopTriggered = false;

    QObject::connect(&manager, &TimelineManager::currentPlayheadTimeChanged, [&]() {
        qint64 playhead = manager.currentPlayheadTime();
        if (playhead >= loopEndUs || playhead < loopStartUs) {
            loopTriggered = true;
            manager.setCurrentPlayheadTime(loopStartUs);
        }
    });

    // Start playback at 1.5s (inside the loop)
    manager.setCurrentPlayheadTime(1500000LL);
    engine.play();

    // Let's advance the audio engine by 1 second (48000 samples)
    // This will push the engine's playback sample past the loop end (1.5s + 1.0s = 2.5s)
    std::vector<float> dummyOut(48000 * 2, 0.0f);
    engine.mixAudio(dummyOut.data(), 48000);

    // Now update playhead from audio. This should trigger the loop!
    QMetaObject::invokeMethod(&engine, "updatePlayheadFromAudio");

    assert(loopTriggered);
    assert(manager.currentPlayheadTime() == 1000000LL);

    // Let's call updatePlayheadFromAudio one more time to verify the engine's internal sample
    // was updated. If it was updated, the engine will compute microseconds as 1000000LL.
    // Therefore, manager.currentPlayheadTime() should remain exactly 1000000LL, and loopTriggered won't be set to true again.
    loopTriggered = false; // Reset to check
    QMetaObject::invokeMethod(&engine, "updatePlayheadFromAudio");
    assert(!loopTriggered); // Should NOT trigger loop again because it's now playing from 1.0s
    assert(manager.currentPlayheadTime() == 1000000LL);

    engine.stop();
    std::cout << "[PASS] Audio Playback Loop test completed successfully.\n\n";
}

int main(int argc, char* argv[]) {
    std::cout << std::unitbuf;
    QCoreApplication app(argc, argv);
    
    std::cout << "========================================================\n";
    std::cout << "        NC-KTV V2 - Timeline Automated Test Suite       \n";
    std::cout << "========================================================\n";
    
    testTimecodeConversions();
    testSnappingEngine();
    testClipSplitting();
    testJsonSaveAndLoad();
    testSourceTrimmingAndOffsets();
    testSyllableParser();
    testLoDPeakGeneration();
    testAudioGainRamping();
    testAudioPlaybackLoop();
    testResampler();
    testOverlapAddWindowing();
    testStemSeparatorFallback();
    testMidSideDspSeparation();
    testOfflineMixdown();
    // testRenderEngineInitialization(); // Commented out to prevent hanging in headless/headless-CI/VM environments without GPU/audio hardware drivers.
    testMarkersAndSubtitleStyles();
    testRomanization();
    testLyricsStringImport();
    testLyricEngine();
    testMetadataGuesser();
    testHwDecodingPreferences();
    testTimelineChangedPropagation();
    testWaveformCaching();
    testBezierTimingCurves();
    testCrossCorrelationAlignment();
    
    std::cout << "========================================================\n";
    std::cout << "       ALL TIMELINE CORE TESTS PASSED SUCCESSFULLY!     \n";
    std::cout << "========================================================\n";
    
    return 0;
}
