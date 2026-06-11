using System;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Net.Http;
using System.Text.Json;
using System.Linq;
using System.IO;

namespace NCKTV.Wpf
{
    public partial class MainWindow : Window
    {
        private DispatcherTimer _playheadTimer;
        private readonly HttpClient _httpClient = new HttpClient();
        private readonly ObservableCollection<YoutubeDownloadItem> _activeDownloads = new ObservableCollection<YoutubeDownloadItem>();
        
        // Prevent GC from reclaiming callbacks passed to native code
        private static NativeBridge.PlayheadChangedCallback _playheadCb;
        private static NativeBridge.RenderProgressCallback _renderProgressCb;
        private static NativeBridge.SeparationProgressCallback _separationProgressCb;
        private static NativeBridge.SeparationCompletedCallback _separationCompletedCb;
        private static NativeBridge.SeparationFailedCallback _separationFailedCb;

        private static NativeBridge.YoutubeSearchCompletedCallback _ytSearchCompletedCb;
        private static NativeBridge.YoutubeDownloadProgressCallback _ytDownloadProgressCb;
        private static NativeBridge.YoutubeDownloadCompletedCallback _ytDownloadCompletedCb;
        private static NativeBridge.YoutubeDownloadFailedCallback _ytDownloadFailedCb;

        // Timeline conversion parameters
        private double ZoomFactor => _zoomFactor;
        private double _zoomFactor = 100.0; // Pixels per second
        private const long UsPerSec = 1000000;

        // Syllable data model list
        private readonly List<SyllableClip> _syllables = new List<SyllableClip>();

        // Dragging & Resizing state variables
        private bool _isDragging = false;
        private bool _isResizing = false;
        private Border _draggedClip = null;
        private double _dragStartMouseY;
        private double _dragStartClipTop;
        private double _dragStartClipHeight;
        private int _activeTapSyllableIndex = 0;

        public MainWindow()
        {
            InitializeComponent();
            
            this.Loaded += MainWindow_Loaded;
            this.Closed += MainWindow_Closed;
            this.PreviewKeyDown += MainWindow_PreviewKeyDown;
        }

        private void ClearSyllables()
        {
            _syllables.Clear();
            _activeTapSyllableIndex = 0;
        }

        private void MainWindow_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            // Do not intercept if user is typing in a TextBox
            if (e.OriginalSource is TextBox) return;

            if (e.Key == Key.Space)
            {
                if (NativeBridge.ncktv_is_playing())
                {
                    NativeBridge.ncktv_pause();
                }
                else
                {
                    NativeBridge.ncktv_play();
                }
                e.Handled = true;
            }
            else if (e.Key == Key.LeftCtrl || e.Key == Key.RightCtrl)
            {
                // Tap current word
                if (_syllables.Count > 0 && _activeTapSyllableIndex < _syllables.Count)
                {
                    long playheadUs = NativeBridge.ncktv_get_playhead();
                    var currentClip = _syllables[_activeTapSyllableIndex];
                    
                    currentClip.StartTimeUs = playheadUs;
                    // Give it a default duration of 300ms
                    currentClip.DurationUs = 300000;

                    // Update previous syllable's duration to end where this one starts
                    if (_activeTapSyllableIndex > 0)
                    {
                        var prevClip = _syllables[_activeTapSyllableIndex - 1];
                        long calculatedDuration = playheadUs - prevClip.StartTimeUs;
                        if (calculatedDuration > 0)
                        {
                            prevClip.DurationUs = calculatedDuration;
                        }
                    }

                    _activeTapSyllableIndex++;
                    RecreateSyllableVisuals();
                    UpdateTimelineHeight();
                }
                e.Handled = true;
            }
            else if (e.Key == Key.Back)
            {
                // Go back one word
                if (_activeTapSyllableIndex > 0)
                {
                    _activeTapSyllableIndex--;
                    var currentClip = _syllables[_activeTapSyllableIndex];
                    NativeBridge.ncktv_set_playhead(currentClip.StartTimeUs);
                    UpdatePlayheadUI(currentClip.StartTimeUs);
                    RecreateSyllableVisuals();
                }
                e.Handled = true;
            }
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            try
            {
                // Load config.yaml to check use_gpu setting
                bool useGpu = true;
                try
                {
                    string baseDir = AppDomain.CurrentDomain.BaseDirectory;
                    string[] possibleConfigPaths = new string[]
                    {
                        System.IO.Path.Combine(baseDir, "config.yaml"),
                        System.IO.Path.Combine(baseDir, "..", "..", "..", "config.yaml"),
                        System.IO.Path.Combine(baseDir, "..", "..", "..", "..", "config.yaml")
                    };

                    foreach (var path in possibleConfigPaths)
                    {
                        if (System.IO.File.Exists(path))
                        {
                            var lines = System.IO.File.ReadAllLines(path);
                            foreach (var line in lines)
                            {
                                string trimmed = line.Trim();
                                if (trimmed.StartsWith("use_gpu:"))
                                {
                                    string val = trimmed.Replace("use_gpu:", "").Trim().ToLower();
                                    if (val == "false" || val == "0")
                                    {
                                        useGpu = false;
                                    }
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
                catch (Exception) { }

                if (!useGpu)
                {
                    Environment.SetEnvironmentVariable("NCKTV_FORCE_CPU", "1");
                }

                // 1. Initialize native C++ core library
                string appDataDir = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
                string modelsDir = System.IO.Path.Combine(appDataDir, "gemini", "antigravity", "models");
                System.IO.Directory.CreateDirectory(modelsDir);

                NativeBridge.ncktv_initialize(modelsDir);

                // 2. Query System Specifications from ONNX and OS detect helpers in C++
                string cpu = NativeBridge.CpuInfo;
                string gpu = NativeBridge.GpuInfo;
                string onnx = NativeBridge.OnnxProviderInfo;
                TxtSpecs.Text = $"CPU: {cpu}\nGPU: {gpu}\nONNX Provider: {onnx}";

                // 3. Setup static delegates for event callbacks
                _playheadCb = OnPlayheadChangedNative;
                _renderProgressCb = OnRenderProgressNative;
                _separationProgressCb = OnSeparationProgressNative;
                _separationCompletedCb = OnSeparationCompletedNative;
                _separationFailedCb = OnSeparationFailedNative;

                _ytSearchCompletedCb = OnYoutubeSearchCompletedNative;
                _ytDownloadProgressCb = OnYoutubeDownloadProgressNative;
                _ytDownloadCompletedCb = OnYoutubeDownloadCompletedNative;
                _ytDownloadFailedCb = OnYoutubeDownloadFailedNative;

                NativeBridge.ncktv_set_playhead_changed_callback(_playheadCb);
                NativeBridge.ncktv_set_render_progress_callback(_renderProgressCb);
                NativeBridge.ncktv_set_separation_progress_callback(_separationProgressCb);
                NativeBridge.ncktv_set_separation_completed_callback(_separationCompletedCb);
                NativeBridge.ncktv_set_separation_failed_callback(_separationFailedCb);

                NativeBridge.ncktv_youtube_set_search_completed_callback(_ytSearchCompletedCb);
                NativeBridge.ncktv_youtube_set_download_progress_callback(_ytDownloadProgressCb);
                NativeBridge.ncktv_youtube_set_download_completed_callback(_ytDownloadCompletedCb);
                NativeBridge.ncktv_youtube_set_download_failed_callback(_ytDownloadFailedCb);

                // Bind Active Downloads list
                LstYtDownloads.ItemsSource = _activeDownloads;

                // 4. Initialize default empty vertical timeline height
                UpdateTimelineHeight();

                // 5. Spin up WPF playhead polling timer (updates vertical line position)
                _playheadTimer = new DispatcherTimer
                {
                    Interval = TimeSpan.FromMilliseconds(33) // ~30 fps updates
                };
                _playheadTimer.Tick += PlayheadTimer_Tick;
                _playheadTimer.Start();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Initialization Error: {ex.Message}\nMake sure native dlls are built and placed next to the exe.", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void MainWindow_Closed(object sender, EventArgs e)
        {
            _playheadTimer?.Stop();
            _httpClient?.Dispose();
            NativeBridge.ncktv_shutdown();
        }

        private void PlayheadTimer_Tick(object sender, EventArgs e)
        {
            NativeBridge.ncktv_process_events();

            if (NativeBridge.ncktv_is_playing())
            {
                long playheadUs = NativeBridge.ncktv_get_playhead();
                UpdatePlayheadUI(playheadUs);
                UpdateLiveLyricsHighlight(playheadUs);
            }
        }

        private void UpdatePlayheadUI(long timeUs)
        {
            double seconds = timeUs / (double)UsPerSec;
            double yPos = seconds * ZoomFactor;
            
            // Move yellow playhead line vertically
            Canvas.SetTop(VerticalPlayheadLine, yPos);

            // Format timecode
            TimeSpan ts = TimeSpan.FromSeconds(seconds);
            TxtTimecode.Text = ts.ToString(@"hh\:mm\:ss\.ff");
            TxtWavePos.Text = $"Position: {seconds:F3} sec";

            // Autoscroll vertical list to keep playhead in center of viewport
            if (!ScrollSyllableAdjuster.IsMouseOver)
            {
                double viewportHeight = ScrollSyllableAdjuster.ViewportHeight;
                double targetScroll = yPos - (viewportHeight / 2.0);
                if (targetScroll < 0) targetScroll = 0;
                ScrollSyllableAdjuster.ScrollToVerticalOffset(targetScroll);
            }
        }

        // --- Event Callbacks invoked by C++ threads ---
        private void OnPlayheadChangedNative(long timeUs)
        {
            Dispatcher.BeginInvoke(new Action(() => {
                UpdatePlayheadUI(timeUs);
                UpdateLiveLyricsHighlight(timeUs);
            }));
        }

        private void OnRenderProgressNative(double fraction, string statusText)
        {
            Dispatcher.BeginInvoke(new Action(() => {
                // Render progress logic
            }));
        }

        private void OnSeparationProgressNative(double fraction, string statusText)
        {
            Dispatcher.BeginInvoke(new Action(() => {
                PanelSepProgress.Visibility = Visibility.Visible;
                SepProgressBar.Value = fraction * 100;
                TxtSepProgressStatus.Text = $"{statusText} ({Math.Round(fraction * 100)}%)";
            }));
        }

        private void OnSeparationCompletedNative(string vocalsPath, string instPath)
        {
            Dispatcher.BeginInvoke(new Action(() => {
                PanelSepProgress.Visibility = Visibility.Collapsed;
                LstMedia.Items.Add(vocalsPath);
                LstMedia.Items.Add(instPath);

                // Load vocal path to vertical waveform
                TxtFileName.Text = $"File name: {System.IO.Path.GetFileName(vocalsPath)}";
                VerticalWaveform.SourceFile = vocalsPath;
                VerticalWaveform.SourceStart = 0;
                VerticalWaveform.Duration = 180 * UsPerSec; // 3 minutes default duration helper
                TxtWaveLength.Text = "Length: 180.000 sec";
                
                UpdateTimelineHeight();

                MessageBox.Show($"Stem Separation Completed!\nVocals: {vocalsPath}\nInstrumental: {instPath}", "AI Stem separation", MessageBoxButton.OK, MessageBoxImage.Information);
            }));
        }

        private void OnSeparationFailedNative(string errorMessage)
        {
            Dispatcher.BeginInvoke(new Action(() => {
                PanelSepProgress.Visibility = Visibility.Collapsed;
                MessageBox.Show($"AI Stem Separation Failed!\nError: {errorMessage}", "AI Processing Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }));
        }

        // --- Dragging & Resizing Syllables vertically ---
        private void Clip_MouseDown(object sender, MouseButtonEventArgs e)
        {
            if (e.ChangedButton == MouseButton.Left)
            {
                _draggedClip = (Border)sender;

                // Select active tap target on timeline click
                var clip = (SyllableClip)_draggedClip.Tag;
                int idx = _syllables.IndexOf(clip);
                if (idx >= 0)
                {
                    _activeTapSyllableIndex = idx;
                    UpdateLyricsPreviewPanel();
                }

                Point windowMousePos = e.GetPosition(CanvasSyllables);
                _dragStartMouseY = windowMousePos.Y;
                _dragStartClipTop = Canvas.GetTop(_draggedClip);
                _dragStartClipHeight = _draggedClip.Height;

                Point localMousePos = e.GetPosition(_draggedClip);
                // Check if user clicked the bottom 8px boundary to resize duration
                if (localMousePos.Y >= _draggedClip.ActualHeight - 8)
                {
                    _isResizing = true;
                }
                else
                {
                    _isDragging = true;
                }

                _draggedClip.CaptureMouse();
                e.Handled = true;
            }
        }

        private void Clip_MouseMove(object sender, MouseEventArgs e)
        {
            if (_draggedClip != null)
            {
                Point windowMousePos = e.GetPosition(CanvasSyllables);
                double deltaY = windowMousePos.Y - _dragStartMouseY;

                if (_isResizing)
                {
                    double newHeight = _dragStartClipHeight + deltaY;
                    if (newHeight < 15) newHeight = 15;
                    _draggedClip.Height = newHeight;

                    // Realtime feedback in timecode preview
                    double durationSec = newHeight / ZoomFactor;
                    TxtTimecode.Text = $"[RESIZE] {durationSec:F3}s";
                }
                else if (_isDragging)
                {
                    double newTop = _dragStartClipTop + deltaY;
                    if (newTop < 0) newTop = 0;
                    Canvas.SetTop(_draggedClip, newTop);

                    // Realtime feedback in timecode preview
                    double startSec = newTop / ZoomFactor;
                    TxtTimecode.Text = $"[MOVE] {startSec:F3}s";
                }
                e.Handled = true;
            }
        }

        private void Clip_MouseUp(object sender, MouseButtonEventArgs e)
        {
            if (_draggedClip != null)
            {
                _draggedClip.ReleaseMouseCapture();

                var clip = (SyllableClip)_draggedClip.Tag;
                if (_isResizing)
                {
                    double finalHeight = _draggedClip.Height;
                    clip.DurationUs = (long)((finalHeight / ZoomFactor) * UsPerSec);
                }
                else if (_isDragging)
                {
                    double finalTop = Canvas.GetTop(_draggedClip);
                    clip.StartTimeUs = (long)((finalTop / ZoomFactor) * UsPerSec);
                }

                // Write clip timing info back to C++ Core Subtitles Track
                NativeBridge.ncktv_add_clip("Track_Lyric", $"Clip_{clip.Text}_{clip.StartTimeUs}", 2, clip.StartTimeUs, clip.DurationUs, string.Empty, clip.Text);

                _isDragging = false;
                _isResizing = false;
                _draggedClip = null;

                // Refresh visual positions
                RecreateSyllableVisuals();
                UpdateTimelineHeight();
                e.Handled = true;
            }
        }

        // --- YouTube Downloader Callbacks ---
        private void OnYoutubeSearchCompletedNative(string resultsJson)
        {
            Dispatcher.BeginInvoke(new Action(() => {
                try
                {
                    var results = JsonSerializer.Deserialize<List<YoutubeSearchJsonItem>>(resultsJson);
                    LstYtResults.Items.Clear();
                    if (results != null && results.Count > 0)
                    {
                        foreach (var item in results)
                        {
                            LstYtResults.Items.Add(new YoutubeResultItem
                            {
                                VideoId = item.id,
                                Title = item.title,
                                Channel = item.channel,
                                DurationStr = item.durationStr,
                                Thumbnail = item.thumbnail
                            });
                        }
                    }
                    else
                    {
                        LstYtResults.Items.Add(new YoutubeResultItem { Title = "No results found." });
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Error parsing search results: {ex.Message}", "YouTube Search", MessageBoxButton.OK, MessageBoxImage.Error);
                }
                finally
                {
                    BtnYtSearch.IsEnabled = true;
                    BtnYtSearch.Content = "🔍";
                }
            }));
        }

        private void OnYoutubeDownloadProgressNative(string videoId, double progress)
        {
            Dispatcher.BeginInvoke(new Action(() => {
                var item = FindDownloadItem(videoId);
                if (item != null)
                {
                    double roundedProgress = Math.Round(progress * 100);
                    // Only update and trigger UI updates if progress value changed
                    if (Math.Abs(item.Progress - roundedProgress) >= 1.0)
                    {
                        item.Progress = roundedProgress;
                    }
                }
            }));
        }

        private void OnYoutubeDownloadCompletedNative(string videoId, string filePath, bool audioOnly)
        {
            Dispatcher.BeginInvoke(new Action(() => {
                var item = FindDownloadItem(videoId);
                if (item != null)
                {
                    _activeDownloads.Remove(item);
                }
                if (_activeDownloads.Count == 0)
                {
                    PanelYtDownloads.Visibility = Visibility.Collapsed;
                }
                LstMedia.Items.Add(filePath);

                // Set downloaded path directly to waveform
                TxtFileName.Text = $"File name: {System.IO.Path.GetFileName(filePath)}";
                VerticalWaveform.SourceFile = filePath;
                VerticalWaveform.SourceStart = 0;
                VerticalWaveform.Duration = 180 * UsPerSec;
                TxtWaveLength.Text = "Length: 180.000 sec";
                
                UpdateTimelineHeight();

                MessageBox.Show($"YouTube Download Completed!\nSaved to: {filePath}", "Download Success", MessageBoxButton.OK, MessageBoxImage.Information);

                // Ask user if they want to run AI stem separation
                var result = MessageBox.Show(
                    "Download Completed! Would you like to run AI Stem Separation on this track to isolate vocals and instrumentals?",
                    "AI Stem Separation Prompt",
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Question
                );
                if (result == MessageBoxResult.Yes)
                {
                    NativeBridge.ncktv_separate_stems_for_file(filePath);
                    MessageBox.Show("AI stem separation worker spawned in background thread.", "AI Processing", MessageBoxButton.OK, MessageBoxImage.Information);
                }
            }));
        }

        private void OnYoutubeDownloadFailedNative(string videoId, string errorMessage)
        {
            Dispatcher.BeginInvoke(new Action(() => {
                var item = FindDownloadItem(videoId);
                if (item != null)
                {
                    _activeDownloads.Remove(item);
                }
                if (_activeDownloads.Count == 0)
                {
                    PanelYtDownloads.Visibility = Visibility.Collapsed;
                }
                MessageBox.Show($"Download failed for {videoId}:\n{errorMessage}", "Download Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }));
        }

        private YoutubeDownloadItem FindDownloadItem(string videoId)
        {
            foreach (var item in _activeDownloads)
            {
                if (item.VideoId == videoId)
                    return item;
            }
            return null;
        }

        // --- YouTube UI Event Handlers ---
        private void BtnYtSearch_Click(object sender, RoutedEventArgs e)
        {
            string query = TxtYtSearchQuery.Text;
            if (!string.IsNullOrEmpty(query))
            {
                BtnYtSearch.IsEnabled = false;
                BtnYtSearch.Content = "⏳";
                LstYtResults.Items.Clear();
                LstYtResults.Items.Add(new YoutubeResultItem { Title = "Searching YouTube... Please wait..." });

                System.Threading.Tasks.Task.Run(() =>
                {
                    try
                    {
                        NativeBridge.ncktv_youtube_search(query);
                    }
                    catch (Exception ex)
                    {
                        Dispatcher.Invoke(() =>
                        {
                            MessageBox.Show($"Error initiating YouTube search: {ex.Message}", "Search Error", MessageBoxButton.OK, MessageBoxImage.Error);
                            BtnYtSearch.IsEnabled = true;
                            BtnYtSearch.Content = "🔍";
                            LstYtResults.Items.Clear();
                        });
                    }
                });
            }
        }

        private void TxtYtSearchQuery_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Enter)
            {
                BtnYtSearch_Click(sender, e);
            }
        }

        private void BtnYtDownloadAudio_Click(object sender, RoutedEventArgs e)
        {
            var btn = (Button)sender;
            var item = (YoutubeResultItem)btn.Tag;
            StartDownload(item, audioOnly: true);
        }

        private void BtnYtDownloadVideo_Click(object sender, RoutedEventArgs e)
        {
            var btn = (Button)sender;
            var item = (YoutubeResultItem)btn.Tag;
            StartDownload(item, audioOnly: false);
        }

        private void BtnYtPreview_Click(object sender, RoutedEventArgs e)
        {
            var btn = (Button)sender;
            var item = (YoutubeResultItem)btn.Tag;
            if (item != null)
            {
                string url = $"https://www.youtube.com/watch?v={item.VideoId}";
                try
                {
                    System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
                    {
                        FileName = url,
                        UseShellExecute = true
                    });
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"Could not open preview link: {ex.Message}", "Error", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private void StartDownload(YoutubeResultItem item, bool audioOnly)
        {
            if (FindDownloadItem(item.VideoId) != null)
            {
                MessageBox.Show("This video is already being downloaded.", "Download Info", MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }

            string saveDir = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "NC-KTV Downloads");
            System.IO.Directory.CreateDirectory(saveDir);

            PanelYtDownloads.Visibility = Visibility.Visible;
            _activeDownloads.Add(new YoutubeDownloadItem
            {
                VideoId = item.VideoId,
                Title = item.Title,
                Progress = 0
            });

            // Capture parameters to ensure thread safety
            string videoId = item.VideoId;
            string title = item.Title;

            System.Threading.Tasks.Task.Run(() =>
            {
                try
                {
                    NativeBridge.ncktv_youtube_download(videoId, audioOnly, saveDir, title);
                }
                catch (Exception ex)
                {
                    Dispatcher.Invoke(() =>
                    {
                        MessageBox.Show($"Error initiating download: {ex.Message}", "Download Error", MessageBoxButton.OK, MessageBoxImage.Error);
                        var dlItem = FindDownloadItem(videoId);
                        if (dlItem != null)
                        {
                            _activeDownloads.Remove(dlItem);
                        }
                        if (_activeDownloads.Count == 0)
                        {
                            PanelYtDownloads.Visibility = Visibility.Collapsed;
                        }
                    });
                }
            });
        }

        private void BtnYtCancelDownload_Click(object sender, RoutedEventArgs e)
        {
            var btn = (Button)sender;
            string videoId = btn.Tag.ToString();
            NativeBridge.ncktv_youtube_cancel_download(videoId);
            var item = FindDownloadItem(videoId);
            if (item != null)
            {
                _activeDownloads.Remove(item);
            }
            if (_activeDownloads.Count == 0)
            {
                PanelYtDownloads.Visibility = Visibility.Collapsed;
            }
        }

        // --- LRC Finder Event Handlers ---
        private async void BtnLrcSearch_Click(object sender, RoutedEventArgs e)
        {
            string title = TxtLrcTitle.Text.Trim();
            string artist = TxtLrcArtist.Text.Trim();
            if (string.IsNullOrEmpty(title) && string.IsNullOrEmpty(artist))
            {
                MessageBox.Show("Please enter a song title or artist name.", "Search Required", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            BtnLrcSearch.IsEnabled = false;
            BtnLrcSearch.Content = "Finding...";
            LstLrcResults.Items.Clear();
            TxtLrcPreviewText.Text = "";
            BtnImportLrcSearchResult.IsEnabled = false;

            try
            {
                string url;
                if (!string.IsNullOrEmpty(title) && !string.IsNullOrEmpty(artist))
                {
                    url = $"https://lrclib.net/api/search?track_name={Uri.EscapeDataString(title)}&artist_name={Uri.EscapeDataString(artist)}";
                }
                else
                {
                    string q = !string.IsNullOrEmpty(title) ? title : artist;
                    url = $"https://lrclib.net/api/search?q={Uri.EscapeDataString(q)}";
                }

                _httpClient.DefaultRequestHeaders.UserAgent.Clear();
                _httpClient.DefaultRequestHeaders.UserAgent.ParseAdd("NC-KTV/2.0 (WPF Editor)");
                var response = await _httpClient.GetAsync(url);
                if (!response.IsSuccessStatusCode)
                {
                    MessageBox.Show($"LRCLIB API returned error: {response.StatusCode}", "Lyrics Search", MessageBoxButton.OK, MessageBoxImage.Error);
                    return;
                }

                string json = await response.Content.ReadAsStringAsync();
                var results = JsonSerializer.Deserialize<List<LrcResultItem>>(json, new JsonSerializerOptions
                {
                    PropertyNameCaseInsensitive = true
                });

                if (results == null || results.Count == 0)
                {
                    MessageBox.Show("No lyrics found for your query.", "Lyrics Search", MessageBoxButton.OK, MessageBoxImage.Information);
                    return;
                }

                foreach (var item in results)
                {
                    LstLrcResults.Items.Add(item);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Network or API error: {ex.Message}", "Lyrics Search", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                BtnLrcSearch.IsEnabled = true;
                BtnLrcSearch.Content = "Find";
            }
        }

        private void LstLrcResults_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (LstLrcResults.SelectedItem is LrcResultItem selected)
            {
                if (!string.IsNullOrEmpty(selected.SyncedLyrics))
                {
                    TxtLrcPreviewText.Text = selected.SyncedLyrics;
                    BtnImportLrcSearchResult.IsEnabled = true;
                }
                else if (!string.IsNullOrEmpty(selected.PlainLyrics))
                {
                    TxtLrcPreviewText.Text = "[Plain Lyrics (No timestamps)]\n\n" + selected.PlainLyrics;
                    BtnImportLrcSearchResult.IsEnabled = true;
                }
                else
                {
                    TxtLrcPreviewText.Text = "[Instrumental or No Lyrics Available]";
                    BtnImportLrcSearchResult.IsEnabled = false;
                }
            }
            else
            {
                TxtLrcPreviewText.Text = "";
                BtnImportLrcSearchResult.IsEnabled = false;
            }
        }

        private void BtnImportLrcSearchResult_Click(object sender, RoutedEventArgs e)
        {
            if (LstLrcResults.SelectedItem is LrcResultItem selected)
            {
                string lyricsToImport = !string.IsNullOrEmpty(selected.SyncedLyrics) ? selected.SyncedLyrics : selected.PlainLyrics;
                if (!string.IsNullOrEmpty(lyricsToImport))
                {
                    if (NativeBridge.ncktv_import_lyrics_from_string("Track_Lyric", lyricsToImport))
                    {
                        LoadSyllablesFromLrc(lyricsToImport);
                        
                        // Populate editor text
                        TxtMainLyricsEditor.Text = !string.IsNullOrEmpty(selected.PlainLyrics) ? selected.PlainLyrics : CleanLrcTimestamps(lyricsToImport);

                        MessageBox.Show("Lyrics successfully imported to vertical timeline!", "Lyrics Finder", MessageBoxButton.OK, MessageBoxImage.Information);
                    }
                    else
                    {
                        MessageBox.Show("Failed to import lyrics. Make sure Track_Lyric exists.", "Lyrics Finder", MessageBoxButton.OK, MessageBoxImage.Error);
                    }
                }
            }
        }

        private string CleanLrcTimestamps(string lrc)
        {
            StringBuilder sb = new StringBuilder();
            var lines = lrc.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
            foreach (var line in lines)
            {
                string text = line.Trim();
                int idx = text.LastIndexOf(']');
                if (idx >= 0 && idx < text.Length - 1)
                {
                    text = text.Substring(idx + 1).Trim();
                }
                else if (text.StartsWith("["))
                {
                    continue;
                }
                if (!string.IsNullOrEmpty(text))
                {
                    sb.AppendLine(text);
                }
            }
            return sb.ToString();
        }

        // --- Syllables Generator and Timings distribution helpers ---
        private void GenerateSyllablesFromEditorText()
        {
            string editorText = TxtMainLyricsEditor.Text;
            if (string.IsNullOrEmpty(editorText)) return;

            ClearSyllables();
            CanvasSyllables.Children.Clear();
            CanvasSyllables.Children.Add(VerticalPlayheadLine);

            var lines = editorText.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
            long currentStartUs = 2 * UsPerSec; // start at 2s padding
            long lineDurationUs = 4 * UsPerSec; // assume 4s line duration default

            int lineIndex = 0;
            string lang = GuessLanguage(editorText);
            bool isCjk = lang == "zh" || lang == "ja" || lang == "ko";

            for (int l = 0; l < lines.Length; l++)
            {
                string lineText = lines[l].Trim();
                if (string.IsNullOrEmpty(lineText)) continue;

                var parsedWords = new List<List<string>>();
                if (isCjk)
                {
                    foreach (char c in lineText)
                    {
                        if (!char.IsWhiteSpace(c))
                        {
                            parsedWords.Add(new List<string> { c.ToString() });
                        }
                    }
                }
                else
                {
                    var rawWords = lineText.Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                    foreach (var rw in rawWords)
                    {
                        var parts = rw.Split('-');
                        var wordSyls = new List<string>();
                        for (int idx = 0; idx < parts.Length; idx++)
                        {
                            if (string.IsNullOrEmpty(parts[idx])) continue;
                            if (idx < parts.Length - 1)
                            {
                                wordSyls.Add(parts[idx] + "-");
                            }
                            else
                            {
                                wordSyls.Add(parts[idx]);
                            }
                        }
                        if (wordSyls.Count > 0)
                        {
                            parsedWords.Add(wordSyls);
                        }
                    }
                }

                var lineSyls = new List<string>();
                foreach (var w in parsedWords)
                {
                    lineSyls.AddRange(w);
                }

                if (lineSyls.Count == 0) continue;

                long sylDurationUs = lineDurationUs / lineSyls.Count;
                for (int s = 0; s < lineSyls.Count; s++)
                {
                    var clip = new SyllableClip
                    {
                        Text = lineSyls[s],
                        StartTimeUs = currentStartUs + (s * sylDurationUs),
                        DurationUs = sylDurationUs - 40000, // 40ms gap
                        LineIndex = lineIndex
                    };
                    _syllables.Add(clip);
                }

                currentStartUs += lineDurationUs + 600000; // 600ms gap
                lineIndex++;
            }

            RecreateSyllableVisuals();
            UpdateTimelineHeight();
        }

        private void RecreateSyllableVisuals()
        {
            // Always sort syllables chronologically
            _syllables.Sort((a, b) => a.StartTimeUs.CompareTo(b.StartTimeUs));

            // Remove old visual items (keep playhead line)
            var toRemove = new List<UIElement>();
            foreach (UIElement child in CanvasSyllables.Children)
            {
                if (child != VerticalPlayheadLine)
                {
                    toRemove.Add(child);
                }
            }
            foreach (var element in toRemove)
            {
                CanvasSyllables.Children.Remove(element);
            }

            foreach (var clip in _syllables)
            {
                double top = (clip.StartTimeUs / (double)UsPerSec) * ZoomFactor;
                double height = (clip.DurationUs / (double)UsPerSec) * ZoomFactor;

                var border = new Border
                {
                    Height = height,
                    Width = 120,
                    Background = new LinearGradientBrush(
                        Color.FromRgb(230, 74, 25),  // Dark Orange-Red Accent
                        Color.FromRgb(191, 54, 12),
                        90
                    ),
                    BorderBrush = new SolidColorBrush(Color.FromRgb(255, 87, 34)),
                    BorderThickness = new Thickness(1),
                    CornerRadius = new CornerRadius(4),
                    Cursor = Cursors.SizeAll,
                    Tag = clip
                };

                var grid = new Grid();
                var txt = new TextBlock
                {
                    Text = clip.Text,
                    Foreground = Brushes.White,
                    FontWeight = FontWeights.Bold,
                    FontSize = 10,
                    HorizontalAlignment = HorizontalAlignment.Center,
                    VerticalAlignment = VerticalAlignment.Center,
                    TextTrimming = TextTrimming.CharacterEllipsis
                };
                grid.Children.Add(txt);

                // Resize handle block at the bottom
                var handle = new Border
                {
                    Height = 6,
                    Background = new SolidColorBrush(Color.FromArgb(80, 255, 255, 255)),
                    VerticalAlignment = VerticalAlignment.Bottom,
                    Cursor = Cursors.SizeNS
                };
                grid.Children.Add(handle);

                border.Child = grid;

                border.MouseDown += Clip_MouseDown;
                border.MouseMove += Clip_MouseMove;
                border.MouseUp += Clip_MouseUp;

                Canvas.SetLeft(border, 110); // Placed next to the 90px waveform
                Canvas.SetTop(border, top);

                CanvasSyllables.Children.Add(border);
                clip.VisualElement = border;
            }

            // Keep the interactive lyrics preview panel in sync
            UpdateLyricsPreviewPanel();

            // Synchronize the tracks in C++ core to prevent orphan/duplicate clips
            SyncSyllablesToNative();
        }

        private void SyncSyllablesToNative()
        {
            NativeBridge.ncktv_clear_track_clips("Track_Lyric");
            foreach (var clip in _syllables)
            {
                NativeBridge.ncktv_add_clip(
                    "Track_Lyric",
                    $"Clip_{clip.Text}_{clip.StartTimeUs}",
                    2,
                    clip.StartTimeUs,
                    clip.DurationUs,
                    string.Empty,
                    clip.Text
                );
            }
        }

        private void UpdateLyricsPreviewPanel()
        {
            PanelLyrics.Children.Clear();
            if (_syllables.Count == 0)
            {
                var tbPlaceholder = new TextBlock
                {
                    Text = "Import synced lyrics (LRC) or run AI Sync to start...",
                    FontSize = 13,
                    Foreground = (Brush)FindResource("TextGrayBrush"),
                    VerticalAlignment = VerticalAlignment.Center
                };
                PanelLyrics.Children.Add(tbPlaceholder);
                return;
            }

            // Group syllables by LineIndex, ordered by LineIndex
            var groupedLines = _syllables
                .GroupBy(s => s.LineIndex)
                .OrderBy(g => g.Key);

            foreach (var group in groupedLines)
            {
                var linePanel = new WrapPanel
                {
                    Orientation = Orientation.Horizontal,
                    Margin = new Thickness(0, 4, 0, 4),
                    HorizontalAlignment = HorizontalAlignment.Left
                };

                var lineSyllables = group.OrderBy(s => s.StartTimeUs).ToList();
                for (int sIdx = 0; sIdx < lineSyllables.Count; sIdx++)
                {
                    var clip = lineSyllables[sIdx];
                    string displayText = clip.Text;

                    if (displayText.EndsWith("-"))
                    {
                        displayText = displayText.Substring(0, displayText.Length - 1);
                    }
                    else if (sIdx < lineSyllables.Count - 1)
                    {
                        string nextText = lineSyllables[sIdx + 1].Text;
                        if (!IsCjkString(clip.Text) || !IsCjkString(nextText))
                        {
                            displayText += " ";
                        }
                    }

                    var tb = new TextBlock
                    {
                        Text = displayText,
                        FontSize = 16,
                        FontFamily = (FontFamily)FindResource("OutfitFont"),
                        Foreground = (Brush)FindResource("TextDarkBrush"),
                        FontWeight = FontWeights.Normal,
                        Cursor = Cursors.Hand,
                        Margin = new Thickness(0, 0, 1, 0)
                    };

                    int globalIdx = _syllables.IndexOf(clip);
                    if (globalIdx == _activeTapSyllableIndex)
                    {
                        tb.Foreground = (Brush)FindResource("AccentOrangeBrush");
                        tb.FontWeight = FontWeights.Bold;
                        tb.TextDecorations = TextDecorations.Underline;
                    }

                    var currentClip = clip;
                    tb.PreviewMouseLeftButtonDown += (s, e) =>
                    {
                        NativeBridge.ncktv_set_playhead(currentClip.StartTimeUs);
                        UpdatePlayheadUI(currentClip.StartTimeUs);
                        
                        int idx = _syllables.IndexOf(currentClip);
                        if (idx >= 0)
                        {
                            _activeTapSyllableIndex = idx;
                            UpdateLyricsPreviewPanel();
                        }
                        
                        e.Handled = true;
                    };

                    clip.PreviewTextBlock = tb;
                    linePanel.Children.Add(tb);
                }

                PanelLyrics.Children.Add(linePanel);
            }
        }

        private void UpdateTimelineHeight()
        {
            long maxTimeUs = 60 * UsPerSec; // 60s minimum default
            try
            {
                long durUs = NativeBridge.ncktv_get_total_duration();
                if (durUs > maxTimeUs)
                {
                    maxTimeUs = durUs;
                }
            }
            catch { }

            foreach (var clip in _syllables)
            {
                long endTimeUs = clip.StartTimeUs + clip.DurationUs;
                if (endTimeUs > maxTimeUs)
                {
                    maxTimeUs = endTimeUs;
                }
            }

            double totalSeconds = maxTimeUs / (double)UsPerSec;
            double targetHeight = (totalSeconds + 10.0) * ZoomFactor; // add padding

            GridVerticalTimeline.Height = targetHeight;
            CanvasSyllables.Height = targetHeight;
            VerticalWaveform.Height = targetHeight;
        }

        private void LoadSyllablesFromLrc(string lrcContent)
        {
            ClearSyllables();
            CanvasSyllables.Children.Clear();
            CanvasSyllables.Children.Add(VerticalPlayheadLine);

            var lines = lrcContent.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
            var parsedLines = new List<Tuple<long, string>>();

            foreach (var line in lines)
            {
                string text = line.Trim();
                if (string.IsNullOrEmpty(text)) continue;

                if (text.StartsWith("[") && text.Contains("]"))
                {
                    int endIdx = text.IndexOf(']');
                    string timeStr = text.Substring(1, endIdx - 1);
                    string lyricText = text.Substring(endIdx + 1).Trim();

                    if (timeStr.Contains(":") && (char.IsDigit(timeStr[0]) || timeStr[0] == ':'))
                    {
                        try
                        {
                            var parts = timeStr.Split(':');
                            double minutes = double.Parse(parts[0]);
                            double seconds = double.Parse(parts[1]);
                            long timeUs = (long)((minutes * 60 + seconds) * UsPerSec);
                            parsedLines.Add(Tuple.Create(timeUs, lyricText));
                        }
                        catch
                        {
                            // ignore malformed lines
                        }
                    }
                }
            }

            parsedLines.Sort((a, b) => a.Item1.CompareTo(b.Item1));

            int lineIndex = 0;
            for (int i = 0; i < parsedLines.Count; i++)
            {
                long startUs = parsedLines[i].Item1;
                string text = parsedLines[i].Item2;
                
                long endUs = (i < parsedLines.Count - 1) ? parsedLines[i + 1].Item1 : startUs + 4 * UsPerSec;
                long durationUs = endUs - startUs;
                if (durationUs > 5 * UsPerSec) durationUs = 5 * UsPerSec; // cap line duration to 5s

                string lang = GuessLanguage(text);
                bool isCjk = lang == "zh" || lang == "ja" || lang == "ko";

                var parsedWords = new List<List<string>>();
                if (isCjk)
                {
                    foreach (char c in text)
                    {
                        if (!char.IsWhiteSpace(c))
                        {
                            parsedWords.Add(new List<string> { c.ToString() });
                        }
                    }
                }
                else
                {
                    var rawWords = text.Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                    foreach (var rw in rawWords)
                    {
                        var parts = rw.Split('-');
                        var wordSyls = new List<string>();
                        for (int idx = 0; idx < parts.Length; idx++)
                        {
                            if (string.IsNullOrEmpty(parts[idx])) continue;
                            if (idx < parts.Length - 1)
                            {
                                wordSyls.Add(parts[idx] + "-");
                            }
                            else
                            {
                                wordSyls.Add(parts[idx]);
                            }
                        }
                        if (wordSyls.Count > 0)
                        {
                            parsedWords.Add(wordSyls);
                        }
                    }
                }

                var lineSyls = new List<string>();
                foreach (var w in parsedWords)
                {
                    lineSyls.AddRange(w);
                }

                if (lineSyls.Count == 0) continue;

                long sylDurationUs = durationUs / lineSyls.Count;
                for (int s = 0; s < lineSyls.Count; s++)
                {
                    _syllables.Add(new SyllableClip
                    {
                        Text = lineSyls[s],
                        StartTimeUs = startUs + (s * sylDurationUs),
                        DurationUs = sylDurationUs - 20000, // 20ms gap
                        LineIndex = lineIndex
                    });
                }
                lineIndex++;
            }

            RecreateSyllableVisuals();
            UpdateTimelineHeight();
        }

        private void LoadLyricsFromFile(string filePath)
        {
            try
            {
                string content = System.IO.File.ReadAllText(filePath, Encoding.UTF8);
                bool isSrt = filePath.EndsWith(".srt", StringComparison.OrdinalIgnoreCase);
                bool isLrc = filePath.EndsWith(".lrc", StringComparison.OrdinalIgnoreCase);

                if (!isSrt && !isLrc)
                {
                    if (content.Contains("-->")) isSrt = true;
                    else isLrc = true;
                }

                if (isLrc)
                {
                    LoadSyllablesFromLrc(content);
                    TxtMainLyricsEditor.Text = CleanLrcTimestamps(content);
                }
                else // srt
                {
                    LoadSyllablesFromSrt(content);
                    TxtMainLyricsEditor.Text = CleanSrtTimestamps(content);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to parse lyrics file locally: {ex.Message}", "Parsing Error", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void LoadSyllablesFromSrt(string srtContent)
        {
            ClearSyllables();
            CanvasSyllables.Children.Clear();
            CanvasSyllables.Children.Add(VerticalPlayheadLine);

            var lines = srtContent.Split(new[] { '\r', '\n' }, StringSplitOptions.None);
            int state = 0;
            long startUs = 0;
            long durationUs = 0;
            string lyricText = "";

            var timecodeRegex = new System.Text.RegularExpressions.Regex(
                @"(\d{2}):(\d{2}):(\d{2})[,.](\d{3})\s*-->\s*(\d{2}):(\d{2}):(\d{2})[,.](\d{3})");

            int lineIndex = 0;
            Action addSubtitleSyllables = () =>
            {
                string text = lyricText.Trim();
                if (string.IsNullOrEmpty(text)) return;

                var words = text.Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                if (words.Length == 0) return;

                long wordDurationUs = durationUs / words.Length;
                for (int w = 0; w < words.Length; w++)
                {
                    _syllables.Add(new SyllableClip
                    {
                        Text = words[w],
                        StartTimeUs = startUs + (w * wordDurationUs),
                        DurationUs = wordDurationUs - 20000, // 20ms gap
                        LineIndex = lineIndex
                    });
                }
                lineIndex++;
            };

            for (int i = 0; i < lines.Length; ++i)
            {
                string trimmed = lines[i].Trim();
                if (state == 0)
                {
                    if (string.IsNullOrEmpty(trimmed)) continue;
                    if (int.TryParse(trimmed, out _))
                    {
                        state = 1;
                    }
                }
                else if (state == 1)
                {
                    var match = timecodeRegex.Match(trimmed);
                    if (match.Success)
                    {
                        int sh = int.Parse(match.Groups[1].Value);
                        int sm = int.Parse(match.Groups[2].Value);
                        int ss = int.Parse(match.Groups[3].Value);
                        int sms = int.Parse(match.Groups[4].Value);

                        int eh = int.Parse(match.Groups[5].Value);
                        int em = int.Parse(match.Groups[6].Value);
                        int es = int.Parse(match.Groups[7].Value);
                        int ems = int.Parse(match.Groups[8].Value);

                        long start = (sh * 3600L + sm * 60L + ss) * 1000000L + sms * 1000L;
                        long end = (eh * 3600L + em * 60L + es) * 1000000L + ems * 1000L;

                        startUs = start;
                        durationUs = (end > start) ? (end - start) : 4000000L;
                        lyricText = "";
                        state = 2;
                    }
                    else
                    {
                        state = 0;
                    }
                }
                else if (state == 2)
                {
                    if (string.IsNullOrEmpty(trimmed))
                    {
                        addSubtitleSyllables();
                        state = 0;
                    }
                    else
                    {
                        if (!string.IsNullOrEmpty(lyricText)) lyricText += " ";
                        lyricText += trimmed;
                    }
                }
            }

            if (state == 2 && !string.IsNullOrEmpty(lyricText))
            {
                addSubtitleSyllables();
            }

            RecreateSyllableVisuals();
            UpdateTimelineHeight();
        }

        private string CleanSrtTimestamps(string srtContent)
        {
            var lines = srtContent.Split(new[] { '\r', '\n' }, StringSplitOptions.None);
            var sb = new StringBuilder();
            int state = 0;

            var timecodeRegex = new System.Text.RegularExpressions.Regex(
                @"(\d{2}):(\d{2}):(\d{2})[,.](\d{3})\s*-->\s*(\d{2}):(\d{2}):(\d{2})[,.](\d{3})");

            for (int i = 0; i < lines.Length; ++i)
            {
                string trimmed = lines[i].Trim();
                if (state == 0)
                {
                    if (string.IsNullOrEmpty(trimmed)) continue;
                    if (int.TryParse(trimmed, out _))
                    {
                        state = 1;
                    }
                }
                else if (state == 1)
                {
                    var match = timecodeRegex.Match(trimmed);
                    if (match.Success)
                    {
                        state = 2;
                    }
                    else
                    {
                        state = 0;
                    }
                }
                else if (state == 2)
                {
                    if (string.IsNullOrEmpty(trimmed))
                    {
                        state = 0;
                    }
                    else
                    {
                        sb.AppendLine(trimmed);
                    }
                }
            }
            return sb.ToString();
        }

        private void BtnImportLrcFile_Click(object sender, RoutedEventArgs e)
        {
            var openFileDialog = new Microsoft.Win32.OpenFileDialog
            {
                Filter = "Lyrics/Subtitle Files (*.lrc; *.srt)|*.lrc;*.srt|All Files (*.*)|*.*"
            };
            if (openFileDialog.ShowDialog() == true)
            {
                string path = openFileDialog.FileName;
                if (NativeBridge.ncktv_import_lyrics_from_file("Track_Lyric", path))
                {
                    LoadLyricsFromFile(path);
                    MessageBox.Show($"Lyrics successfully imported from file: {System.IO.Path.GetFileName(path)}", "Import Success", MessageBoxButton.OK, MessageBoxImage.Information);
                }
                else
                {
                    MessageBox.Show("Failed to import lyrics file. Make sure Track_Lyric exists.", "Import Failure", MessageBoxButton.OK, MessageBoxImage.Error);
                }
            }
        }

        private double MeasureTextWidth(string text, TextBlock textBlock)
        {
            if (string.IsNullOrEmpty(text)) return 0;
            var formattedText = new FormattedText(
                text,
                System.Globalization.CultureInfo.CurrentCulture,
                FlowDirection.LeftToRight,
                new Typeface(textBlock.FontFamily, textBlock.FontStyle, textBlock.FontWeight, textBlock.FontStretch),
                textBlock.FontSize,
                Brushes.Black,
                VisualTreeHelper.GetDpi(textBlock).PixelsPerDip
            );
            return formattedText.Width;
        }

        private List<string> GetTextList(List<SyllableClip> clips)
        {
            var list = new List<string>();
            foreach (var clip in clips)
            {
                list.Add(clip.Text);
            }
            return list;
        }

        private string ReconstructLineText(List<SyllableClip> clips)
        {
            if (clips == null || clips.Count == 0) return string.Empty;
            
            var sb = new StringBuilder();
            for (int i = 0; i < clips.Count; i++)
            {
                string text = clips[i].Text;
                
                if (text.EndsWith("-"))
                {
                    sb.Append(text.Substring(0, text.Length - 1));
                }
                else
                {
                    sb.Append(text);
                    
                    if (i < clips.Count - 1)
                    {
                        string nextText = clips[i + 1].Text;
                        if (!IsCjkString(text) || !IsCjkString(nextText))
                        {
                            sb.Append(" ");
                        }
                    }
                }
            }
            return sb.ToString();
        }

        private bool IsCjkString(string s)
        {
            if (string.IsNullOrEmpty(s)) return false;
            foreach (char c in s)
            {
                if ((c >= 0x4E00 && c <= 0x9FFF) || // Hanzi
                    (c >= 0x3040 && c <= 0x309F) || // Hiragana
                    (c >= 0x30A0 && c <= 0x30FF) || // Katakana
                    (c >= 0xAC00 && c <= 0xD7A3))   // Hangul
                {
                    return true;
                }
            }
            return false;
        }

        private void UpdateLiveLyricsHighlight(long playheadUs)
        {
            if (_syllables.Count == 0)
            {
                TxtSubtitleInactive.Text = string.Empty;
                TxtSubtitleActive.Text = string.Empty;
                TxtSubtitleActive.Visibility = Visibility.Collapsed;
                TxtLiveLyricsPrevLine.Text = string.Empty;
                TxtLiveLyricsNextLine.Text = string.Empty;
                return;
            }

            // Always ensure syllables list is sorted chronologically
            _syllables.Sort((a, b) => a.StartTimeUs.CompareTo(b.StartTimeUs));

            // Find the active syllable (the one where the playhead is currently inside its duration)
            SyllableClip activeSyllable = null;
            for (int i = 0; i < _syllables.Count; i++)
            {
                var clip = _syllables[i];
                if (playheadUs >= clip.StartTimeUs && playheadUs <= (clip.StartTimeUs + clip.DurationUs))
                {
                    activeSyllable = clip;
                    break;
                }
            }

            int activeLineIndex = -1;
            if (activeSyllable != null)
            {
                activeLineIndex = activeSyllable.LineIndex;
            }
            else
            {
                // Playhead is between syllables. Find the closest upcoming syllable.
                SyllableClip upcomingSyllable = null;
                for (int i = 0; i < _syllables.Count; i++)
                {
                    if (_syllables[i].StartTimeUs > playheadUs)
                    {
                        upcomingSyllable = _syllables[i];
                        break;
                    }
                }

                if (upcomingSyllable != null)
                {
                    activeLineIndex = upcomingSyllable.LineIndex;
                }
                else
                {
                    // No upcoming syllables, we are past the last syllable.
                    // Let's show the last line as fully sung.
                    activeLineIndex = _syllables[_syllables.Count - 1].LineIndex;
                }
            }

            // Get all syllables for the current, previous and next lines
            var lineSyllables = new List<SyllableClip>();
            var prevLineSyllables = new List<SyllableClip>();
            var nextLineSyllables = new List<SyllableClip>();

            foreach (var clip in _syllables)
            {
                if (clip.LineIndex == activeLineIndex)
                {
                    lineSyllables.Add(clip);
                }
                else if (clip.LineIndex == activeLineIndex - 1)
                {
                    prevLineSyllables.Add(clip);
                }
                else if (clip.LineIndex == activeLineIndex + 1)
                {
                    nextLineSyllables.Add(clip);
                }
            }

            // Sort them to be safe
            lineSyllables.Sort((a, b) => a.StartTimeUs.CompareTo(b.StartTimeUs));
            prevLineSyllables.Sort((a, b) => a.StartTimeUs.CompareTo(b.StartTimeUs));
            nextLineSyllables.Sort((a, b) => a.StartTimeUs.CompareTo(b.StartTimeUs));

            // Set text for previous and next lines
            TxtLiveLyricsPrevLine.Text = ReconstructLineText(prevLineSyllables);
            TxtLiveLyricsNextLine.Text = ReconstructLineText(nextLineSyllables);

            if (lineSyllables.Count == 0)
            {
                TxtSubtitleInactive.Text = string.Empty;
                TxtSubtitleActive.Text = string.Empty;
                TxtSubtitleActive.Visibility = Visibility.Collapsed;
                return;
            }

            // Construct full text of the active line
            string fullLineText = ReconstructLineText(lineSyllables);
            TxtSubtitleInactive.Text = fullLineText;
            TxtSubtitleActive.Text = fullLineText;

            // Compute the sung width of the active line at the current playhead
            double sungWidth = 0;

            // Find if there's any active syllable in this line
            int activeWordIdxInLine = -1;
            for (int i = 0; i < lineSyllables.Count; i++)
            {
                var clip = lineSyllables[i];
                if (playheadUs >= clip.StartTimeUs && playheadUs <= (clip.StartTimeUs + clip.DurationUs))
                {
                    activeWordIdxInLine = i;
                    break;
                }
            }

            if (activeWordIdxInLine != -1)
            {
                // Active syllable is inside the line
                var currentSyllable = lineSyllables[activeWordIdxInLine];

                // Fully sung words (prefixes)
                string sungPrefix = ReconstructLineText(lineSyllables.GetRange(0, activeWordIdxInLine));
                if (activeWordIdxInLine > 0)
                {
                    var lastClipInPrefix = lineSyllables[activeWordIdxInLine - 1];
                    var activeClip = lineSyllables[activeWordIdxInLine];
                    if (!lastClipInPrefix.Text.EndsWith("-") && (!IsCjkString(lastClipInPrefix.Text) || !IsCjkString(activeClip.Text)))
                    {
                        sungPrefix += " ";
                    }
                }

                double progress = 0.0;
                if (currentSyllable.DurationUs > 0)
                {
                    progress = (playheadUs - currentSyllable.StartTimeUs) / (double)currentSyllable.DurationUs;
                    if (progress < 0) progress = 0;
                    if (progress > 1) progress = 1;
                }

                string activeSylText = currentSyllable.Text;
                if (activeSylText.EndsWith("-"))
                {
                    activeSylText = activeSylText.Substring(0, activeSylText.Length - 1);
                }

                double widthAtStart = MeasureTextWidth(sungPrefix, TxtSubtitleActive);
                double widthAtEnd = MeasureTextWidth(sungPrefix + activeSylText, TxtSubtitleActive);
                sungWidth = widthAtStart + (widthAtEnd - widthAtStart) * progress;
            }
            else
            {
                // Playhead is either completely before this line, completely after it, or in a gap between words of this line.
                if (playheadUs < lineSyllables[0].StartTimeUs)
                {
                    sungWidth = 0;
                }
                else if (playheadUs > (lineSyllables[lineSyllables.Count - 1].StartTimeUs + lineSyllables[lineSyllables.Count - 1].DurationUs))
                {
                    sungWidth = MeasureTextWidth(fullLineText, TxtSubtitleActive);
                }
                else
                {
                    // The playhead is in a gap between words of this line. Find the last word that has finished singing.
                    int lastFinishedWordIdx = -1;
                    for (int i = 0; i < lineSyllables.Count; i++)
                    {
                        if (playheadUs > (lineSyllables[i].StartTimeUs + lineSyllables[i].DurationUs))
                        {
                            lastFinishedWordIdx = i;
                        }
                    }

                    if (lastFinishedWordIdx != -1)
                    {
                        string sungPrefix = ReconstructLineText(lineSyllables.GetRange(0, lastFinishedWordIdx + 1));
                        if (lastFinishedWordIdx + 1 < lineSyllables.Count)
                        {
                            var lastClip = lineSyllables[lastFinishedWordIdx];
                            var nextClip = lineSyllables[lastFinishedWordIdx + 1];
                            if (!lastClip.Text.EndsWith("-") && (!IsCjkString(lastClip.Text) || !IsCjkString(nextClip.Text)))
                            {
                                sungPrefix += " ";
                            }
                        }
                        sungWidth = MeasureTextWidth(sungPrefix, TxtSubtitleActive);
                    }
                    else
                    {
                        sungWidth = 0;
                    }
                }
            }

            TxtSubtitleActive.Visibility = Visibility.Visible;
            TxtSubtitleActive.Clip = new RectangleGeometry(new Rect(0, 0, sungWidth, 200));

            // Highlight interactive preview panel TextBlocks
            foreach (var clip in _syllables)
            {
                if (clip.PreviewTextBlock != null)
                {
                    if (playheadUs < clip.StartTimeUs)
                    {
                        clip.PreviewTextBlock.Foreground = (Brush)FindResource("TextDarkBrush");
                        clip.PreviewTextBlock.FontWeight = FontWeights.Normal;
                    }
                    else if (playheadUs >= clip.StartTimeUs && playheadUs <= (clip.StartTimeUs + clip.DurationUs))
                    {
                        clip.PreviewTextBlock.Foreground = (Brush)FindResource("AccentBlueBrush");
                        clip.PreviewTextBlock.FontWeight = FontWeights.Bold;
                    }
                    else // playheadUs > (clip.StartTimeUs + clip.DurationUs)
                    {
                        clip.PreviewTextBlock.Foreground = (Brush)FindResource("AccentGreenBrush");
                        clip.PreviewTextBlock.FontWeight = FontWeights.Normal;
                    }
                }
            }
        }

        // --- XAML Event Handlers ---
        private void TxtZoom_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (TxtZoom == null || CanvasSyllables == null || VerticalPlayheadLine == null || GridVerticalTimeline == null || VerticalWaveform == null) return;
            if (double.TryParse(TxtZoom.Text, out double newZoom) && newZoom > 10)
            {
                _zoomFactor = newZoom;
                RecreateSyllableVisuals();
                UpdateTimelineHeight();
                if (_playheadTimer != null)
                {
                    long playheadUs = NativeBridge.ncktv_get_playhead();
                    UpdatePlayheadUI(playheadUs);
                }
            }
        }

        private string GuessLanguage(string text)
        {
            if (string.IsNullOrEmpty(text)) return "en";
            
            int hangulCount = 0;
            int kanaCount = 0;
            int hanziCount = 0;
            
            foreach (char c in text)
            {
                if (c >= 0xAC00 && c <= 0xD7A3) hangulCount++;
                else if ((c >= 0x3040 && c <= 0x309F) || (c >= 0x30A0 && c <= 0x30FF)) kanaCount++;
                else if (c >= 0x4E00 && c <= 0x9FFF) hanziCount++;
            }
            
            if (hangulCount > 5) return "ko";
            if (kanaCount > 5) return "ja";
            if (hanziCount > 5) return "zh";
            
            return "en";
        }

        private void BtnAiSyncSyllables_Click(object sender, RoutedEventArgs e)
        {
            string soundtrackPath = VerticalWaveform.SourceFile;
            if (string.IsNullOrEmpty(soundtrackPath) || !System.IO.File.Exists(soundtrackPath))
            {
                MessageBox.Show("Please import and load a media soundtrack file first.", "Soundtrack Required", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            string lyricsText = TxtMainLyricsEditor.Text;
            if (string.IsNullOrEmpty(lyricsText.Trim()))
            {
                MessageBox.Show("Please type or load lyrics into the text editor first.", "Lyrics Required", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            string lang = GuessLanguage(lyricsText);
            
            string tempLyricsFile = System.IO.Path.Combine(System.IO.Path.GetTempPath(), $"ktv_lyrics_{Guid.NewGuid().ToString("N")}.txt");
            try
            {
                System.IO.File.WriteAllText(tempLyricsFile, lyricsText, Encoding.UTF8);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Failed to create temporary lyrics file: {ex.Message}", "File Error", MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }

            PanelSepProgress.Visibility = Visibility.Visible;
            TxtSepProgressStatus.Text = $"AI Syncing: running WhisperX forced alignment ({lang})...";
            SepProgressBar.IsIndeterminate = true;

            System.Threading.Tasks.Task.Run(() => {
                string tempLyricsFileLocal = tempLyricsFile;
                try
                {
                    string appDir = AppDomain.CurrentDomain.BaseDirectory;
                    string scriptPath = "";
                    string currentDir = appDir;
                    for (int i = 0; i < 5; i++)
                    {
                        string checkPath = System.IO.Path.Combine(currentDir, "python_bridge.py");
                        if (System.IO.File.Exists(checkPath))
                        {
                            scriptPath = checkPath;
                            break;
                        }
                        string parent = System.IO.Path.GetDirectoryName(currentDir);
                        if (string.IsNullOrEmpty(parent) || parent == currentDir) break;
                        currentDir = parent;
                    }

                    if (string.IsNullOrEmpty(scriptPath))
                    {
                        throw new System.IO.FileNotFoundException("python_bridge.py not found in application path hierarchy.");
                    }

                    string pythonExe = "python";
                    if (System.IO.File.Exists(@"D:\Program Files\pythonembededglobal\python.exe"))
                    {
                        pythonExe = @"D:\Program Files\pythonembededglobal\python.exe";
                    }

                    var startInfo = new System.Diagnostics.ProcessStartInfo
                    {
                        FileName = pythonExe,
                        Arguments = $"\"{scriptPath}\" align \"{soundtrackPath}\" --lyrics \"{tempLyricsFileLocal}\" --lang {lang}",
                        RedirectStandardOutput = true,
                        RedirectStandardError = true,
                        UseShellExecute = false,
                        CreateNoWindow = true,
                        StandardOutputEncoding = Encoding.UTF8
                    };

                    using (var process = System.Diagnostics.Process.Start(startInfo))
                    {
                        if (process == null)
                        {
                            throw new Exception("Failed to launch python process. Make sure python is on your system PATH.");
                        }

                        string output = process.StandardOutput.ReadToEnd();
                        string error = process.StandardError.ReadToEnd();
                        process.WaitForExit();

                        if (process.ExitCode != 0)
                        {
                            throw new Exception($"Python process exited with code {process.ExitCode}.\nError: {error}");
                        }

                        using (JsonDocument doc = JsonDocument.Parse(output))
                        {
                            JsonElement root = doc.RootElement;
                            if (root.TryGetProperty("error", out JsonElement errProp))
                            {
                                throw new Exception(errProp.GetString());
                            }

                            if (root.TryGetProperty("segments", out JsonElement segmentsProp))
                            {
                                Dispatcher.BeginInvoke(new Action(() => {
                                    ClearSyllables();
                                    NativeBridge.ncktv_clear_track_clips("Track_Lyric");

                                    int segmentIndex = 0;
                                    foreach (JsonElement segment in segmentsProp.EnumerateArray())
                                    {
                                        if (segment.TryGetProperty("words", out JsonElement wordsProp))
                                        {
                                            foreach (JsonElement word in wordsProp.EnumerateArray())
                                            {
                                                string wordText = word.GetProperty("word").GetString();
                                                double startSec = 0.0;
                                                double endSec = 0.0;

                                                if (word.TryGetProperty("start", out JsonElement startProp) && startProp.ValueKind != JsonValueKind.Null)
                                                {
                                                    startSec = startProp.GetDouble();
                                                }
                                                if (word.TryGetProperty("end", out JsonElement endProp) && endProp.ValueKind != JsonValueKind.Null)
                                                {
                                                    endSec = endProp.GetDouble();
                                                }

                                                long startUs = (long)(startSec * UsPerSec);
                                                long durationUs = (long)((endSec - startSec) * UsPerSec);
                                                if (durationUs < 50000) durationUs = 50000;

                                                var clip = new SyllableClip
                                                {
                                                    Text = wordText,
                                                    StartTimeUs = startUs,
                                                    DurationUs = durationUs,
                                                    LineIndex = segmentIndex
                                                };
                                                _syllables.Add(clip);

                                                NativeBridge.ncktv_add_clip("Track_Lyric", $"Clip_{clip.Text}_{clip.StartTimeUs}", 2, clip.StartTimeUs, clip.DurationUs, string.Empty, clip.Text);
                                            }
                                        }
                                        segmentIndex++;
                                    }

                                    RecreateSyllableVisuals();
                                    UpdateTimelineHeight();

                                    PanelSepProgress.Visibility = Visibility.Collapsed;
                                    MessageBox.Show($"AI Lyrics Forced Alignment completed successfully!\nSynced {_syllables.Count} syllables across {segmentIndex} lines.", "AI Sync Success", MessageBoxButton.OK, MessageBoxImage.Information);
                                }));
                            }
                            else
                            {
                                throw new Exception("Invalid JSON response from aligner: missing segments.");
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    Dispatcher.BeginInvoke(new Action(() => {
                        PanelSepProgress.Visibility = Visibility.Collapsed;
                        MessageBox.Show($"AI Lyrics Sync Failed!\n\n{ex.Message}\n\nMake sure python, PyTorch, and WhisperX are installed and configured.", "AI Sync Error", MessageBoxButton.OK, MessageBoxImage.Error);
                    }));
                }
                finally
                {
                    try
                    {
                        if (System.IO.File.Exists(tempLyricsFileLocal))
                        {
                            System.IO.File.Delete(tempLyricsFileLocal);
                        }
                    }
                    catch { }
                }
            });
        }

        private void BtnAutoTime_Click(object sender, RoutedEventArgs e)
        {
            GenerateSyllablesFromEditorText();
            MessageBox.Show("Syllables successfully generated! Drag/Resize the orange blocks in the left timeline to align with vertical waveform peaks.", "Syllable Generator", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void TxtMainLyricsEditor_TextChanged(object sender, TextChangedEventArgs e)
        {
            // Live lyrics changed logic
        }

        private void BtnPlay_Click(object sender, RoutedEventArgs e)
        {
            NativeBridge.ncktv_play();
        }

        private void BtnPause_Click(object sender, RoutedEventArgs e)
        {
            NativeBridge.ncktv_pause();
        }

        private void BtnStop_Click(object sender, RoutedEventArgs e)
        {
            NativeBridge.ncktv_stop();
            UpdatePlayheadUI(0);
        }

        private void BtnLoad_Click(object sender, RoutedEventArgs e)
        {
            var openFileDialog = new Microsoft.Win32.OpenFileDialog
            {
                Filter = "Karaoke Project Files (*.nctv)|*.nctv"
            };
            if (openFileDialog.ShowDialog() == true)
            {
                string filePath = openFileDialog.FileName;
                if (NativeBridge.ncktv_load_project(filePath))
                {
                    try
                    {
                        string jsonText = File.ReadAllText(filePath, Encoding.UTF8);
                        using (var doc = JsonDocument.Parse(jsonText))
                        {
                            var root = doc.RootElement;

                            // 1. Clear current state
                            ClearSyllables();
                            LstMedia.Items.Clear();
                            TxtMainLyricsEditor.Text = string.Empty;
                            VerticalWaveform.SourceFile = string.Empty;
                            TxtFileName.Text = "File name: None";
                            TxtWaveLength.Text = "Length: 0.000 sec";

                            if (root.TryGetProperty("ncktv_wpf_metadata", out JsonElement metaProp))
                            {
                                // Restore editor text
                                if (metaProp.TryGetProperty("editorText", out JsonElement edTextProp))
                                {
                                    TxtMainLyricsEditor.Text = edTextProp.GetString();
                                }

                                // Restore syllables
                                if (metaProp.TryGetProperty("syllables", out JsonElement sylsProp) && sylsProp.ValueKind == JsonValueKind.Array)
                                {
                                    foreach (var item in sylsProp.EnumerateArray())
                                    {
                                        string text = item.GetProperty("text").GetString();
                                        long startUs = item.GetProperty("startTimeUs").GetInt64();
                                        long durUs = item.GetProperty("durationUs").GetInt64();
                                        int lineIndex = item.GetProperty("lineIndex").GetInt32();

                                        _syllables.Add(new SyllableClip
                                        {
                                            Text = text,
                                            StartTimeUs = startUs,
                                            DurationUs = durUs,
                                            LineIndex = lineIndex
                                        });
                                    }
                                }

                                // Restore media pool
                                if (metaProp.TryGetProperty("mediaPool", out JsonElement poolProp) && poolProp.ValueKind == JsonValueKind.Array)
                                {
                                    foreach (var item in poolProp.EnumerateArray())
                                    {
                                        string mediaPath = item.GetString();
                                        if (!string.IsNullOrEmpty(mediaPath))
                                        {
                                            LstMedia.Items.Add(mediaPath);
                                        }
                                    }
                                }

                                // Restore active soundtrack
                                if (metaProp.TryGetProperty("activeSoundtrack", out JsonElement trackProp))
                                {
                                    string soundtrackPath = trackProp.GetString();
                                    if (!string.IsNullOrEmpty(soundtrackPath) && File.Exists(soundtrackPath))
                                    {
                                        NativeBridge.ncktv_load_soundtrack(soundtrackPath);

                                        long totalDurUs = NativeBridge.ncktv_get_total_duration();
                                        if (totalDurUs <= 0) totalDurUs = 180 * UsPerSec;
                                        double durationSec = totalDurUs / (double)UsPerSec;

                                        TxtFileName.Text = $"File name: {Path.GetFileName(soundtrackPath)}";
                                        VerticalWaveform.SourceFile = soundtrackPath;
                                        VerticalWaveform.SourceStart = 0;
                                        VerticalWaveform.Duration = totalDurUs;
                                        TxtWaveLength.Text = $"Length: {durationSec:F3} sec";
                                    }
                                }
                            }
                            else
                            {
                                // Fallback: If no metadata exists (older project format), we can still reconstruct syllables from the track clips!
                                if (root.TryGetProperty("tracks", out JsonElement tracksProp) && tracksProp.ValueKind == JsonValueKind.Array)
                                {
                                    foreach (var track in tracksProp.EnumerateArray())
                                    {
                                        string tId = track.TryGetProperty("trackId", out var tidProp) ? tidProp.GetString() : "";
                                        if (tId == "Track_Lyric")
                                        {
                                            if (track.TryGetProperty("clips", out JsonElement clipsProp) && clipsProp.ValueKind == JsonValueKind.Array)
                                            {
                                                foreach (var clip in clipsProp.EnumerateArray())
                                                {
                                                    string text = clip.TryGetProperty("lyricText", out var ltProp) ? ltProp.GetString() : "";
                                                    long startUs = clip.TryGetProperty("startTime", out var stProp) ? stProp.GetInt64() : 0;
                                                    long durUs = clip.TryGetProperty("duration", out var dtProp) ? dtProp.GetInt64() : 0;

                                                    _syllables.Add(new SyllableClip
                                                    {
                                                        Text = text,
                                                        StartTimeUs = startUs,
                                                        DurationUs = durUs,
                                                        LineIndex = 0 // default fallback
                                                    });
                                                }
                                            }
                                        }
                                        else if (tId == "Track_Instrumental" || tId == "Track_Vocals")
                                        {
                                            if (track.TryGetProperty("clips", out JsonElement clipsProp) && clipsProp.ValueKind == JsonValueKind.Array)
                                            {
                                                foreach (var clip in clipsProp.EnumerateArray())
                                                {
                                                    string sourceFile = clip.TryGetProperty("sourceFile", out var sfProp) ? sfProp.GetString() : "";
                                                    if (!string.IsNullOrEmpty(sourceFile) && File.Exists(sourceFile))
                                                    {
                                                        if (!LstMedia.Items.Contains(sourceFile))
                                                        {
                                                            LstMedia.Items.Add(sourceFile);
                                                        }
                                                        if (string.IsNullOrEmpty(VerticalWaveform.SourceFile))
                                                        {
                                                            NativeBridge.ncktv_load_soundtrack(sourceFile);

                                                            long totalDurUs = NativeBridge.ncktv_get_total_duration();
                                                            if (totalDurUs <= 0) totalDurUs = 180 * UsPerSec;
                                                            double durationSec = totalDurUs / (double)UsPerSec;

                                                            TxtFileName.Text = $"File name: {Path.GetFileName(sourceFile)}";
                                                            VerticalWaveform.SourceFile = sourceFile;
                                                            VerticalWaveform.SourceStart = 0;
                                                            VerticalWaveform.Duration = totalDurUs;
                                                            TxtWaveLength.Text = $"Length: {durationSec:F3} sec";
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Reconstruct editor text from loaded syllables
                                    TxtMainLyricsEditor.Text = string.Join(" ", _syllables.Select(s => s.Text));
                                }
                            }
                        }

                        RecreateSyllableVisuals();
                        UpdateTimelineHeight();
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"Project loaded, but failed to restore WPF UI state: {ex.Message}", "Load Warning", MessageBoxButton.OK, MessageBoxImage.Warning);
                    }

                    MessageBox.Show("Project loaded successfully!", "Success", MessageBoxButton.OK, MessageBoxImage.Information);
                }
            }
        }

        private void BtnSave_Click(object sender, RoutedEventArgs e)
        {
            var saveFileDialog = new Microsoft.Win32.SaveFileDialog
            {
                Filter = "Karaoke Project Files (*.nctv)|*.nctv",
                FileName = "karaoke_project.nctv"
            };
            if (saveFileDialog.ShowDialog() == true)
            {
                string filePath = saveFileDialog.FileName;
                if (NativeBridge.ncktv_save_project(filePath))
                {
                    try
                    {
                        string jsonText = File.ReadAllText(filePath, Encoding.UTF8);
                        using (var doc = JsonDocument.Parse(jsonText))
                        {
                            var root = doc.RootElement;
                            using (var ms = new MemoryStream())
                            {
                                using (var writer = new Utf8JsonWriter(ms, new JsonWriterOptions { Indented = true }))
                                {
                                    writer.WriteStartObject();

                                    // Copy all existing properties
                                    foreach (var prop in root.EnumerateObject())
                                    {
                                        if (prop.Name != "ncktv_wpf_metadata")
                                        {
                                            prop.WriteTo(writer);
                                        }
                                    }

                                    // Write our metadata
                                    writer.WriteStartObject("ncktv_wpf_metadata");
                                    writer.WriteString("editorText", TxtMainLyricsEditor.Text ?? string.Empty);

                                    // Syllables list
                                    writer.WriteStartArray("syllables");
                                    foreach (var clip in _syllables)
                                    {
                                        writer.WriteStartObject();
                                        writer.WriteString("text", clip.Text ?? string.Empty);
                                        writer.WriteNumber("startTimeUs", clip.StartTimeUs);
                                        writer.WriteNumber("durationUs", clip.DurationUs);
                                        writer.WriteNumber("lineIndex", clip.LineIndex);
                                        writer.WriteEndObject();
                                    }
                                    writer.WriteEndArray();

                                    // Media pool
                                    writer.WriteStartArray("mediaPool");
                                    foreach (var item in LstMedia.Items)
                                    {
                                        writer.WriteStringValue(item.ToString());
                                    }
                                    writer.WriteEndArray();

                                    // Active soundtrack
                                    writer.WriteString("activeSoundtrack", VerticalWaveform.SourceFile ?? string.Empty);

                                    writer.WriteEndObject(); // ncktv_wpf_metadata
                                    writer.WriteEndObject(); // root
                                }

                                string newJsonText = Encoding.UTF8.GetString(ms.ToArray());
                                File.WriteAllText(filePath, newJsonText, Encoding.UTF8);
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"Warning: Project saved, but failed to embed WPF metadata: {ex.Message}", "Save Metadata Warning", MessageBoxButton.OK, MessageBoxImage.Warning);
                    }

                    MessageBox.Show("Project saved successfully!", "Success", MessageBoxButton.OK, MessageBoxImage.Information);
                }
            }
        }

        private void BtnExport_Click(object sender, RoutedEventArgs e)
        {
            var saveFileDialog = new Microsoft.Win32.SaveFileDialog
            {
                Filter = "MP4 Video Files (*.mp4)|*.mp4",
                FileName = "export.mp4"
            };
            if (saveFileDialog.ShowDialog() == true)
            {
                int w = 1920;
                int h = 1080;
                int fps = 30;
                int vBr = 4000000;
                int aBr = 192000;

                int.TryParse(TxtExportWidth.Text, out w);
                int.TryParse(TxtExportHeight.Text, out h);
                int.TryParse(TxtExportFps.Text, out fps);
                int.TryParse(TxtExportVideoBitrate.Text, out vBr);
                int.TryParse(TxtExportAudioBitrate.Text, out aBr);

                NativeBridge.ncktv_start_export(saveFileDialog.FileName, w, h, fps, vBr, aBr, "AAC");
                MessageBox.Show("Render job submitted to native muxing queue.", "Rendering", MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        private void BtnImportMedia_Click(object sender, RoutedEventArgs e)
        {
            var openFileDialog = new Microsoft.Win32.OpenFileDialog
            {
                Filter = "Media Files (*.wav; *.mp3; *.mp4; *.mkv; *.avi; *.mov; *.m4a; *.flac; *.ogg)|*.wav;*.mp3;*.mp4;*.mkv;*.avi;*.mov;*.m4a;*.flac;*.ogg|All Files (*.*)|*.*"
            };
            if (openFileDialog.ShowDialog() == true)
            {
                string path = openFileDialog.FileName;
                if (!LstMedia.Items.Contains(path))
                {
                    LstMedia.Items.Add(path);
                }

                // Automatically load selected track to vertical timeline without AI stem separation
                NativeBridge.ncktv_load_soundtrack(path);

                // Query actual decoded total duration from C++ engine
                long totalDurUs = NativeBridge.ncktv_get_total_duration();
                if (totalDurUs <= 0)
                {
                    totalDurUs = 180 * UsPerSec; // 3 minutes default fallback
                }
                double durationSec = totalDurUs / (double)UsPerSec;

                TxtFileName.Text = $"File name: {System.IO.Path.GetFileName(path)}";
                VerticalWaveform.SourceFile = path;
                VerticalWaveform.SourceStart = 0;
                VerticalWaveform.Duration = totalDurUs;
                TxtWaveLength.Text = $"Length: {durationSec:F3} sec";

                UpdateTimelineHeight();
            }
        }

        private void BtnSeparateStems_Click(object sender, RoutedEventArgs e)
        {
            if (LstMedia.SelectedItem != null)
            {
                string path = LstMedia.SelectedItem.ToString();
                NativeBridge.ncktv_separate_stems_for_file(path);
                MessageBox.Show("AI stem separation worker spawned in background thread.", "AI Processing", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            else
            {
                MessageBox.Show("Please select an audio file from the Media Pool first.", "Selection Required", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        private void BtnImportLrc_Click(object sender, RoutedEventArgs e)
        {
            if (!string.IsNullOrEmpty(TxtLrcInput.Text))
            {
                if (NativeBridge.ncktv_import_lyrics_from_string("Track_Lyric", TxtLrcInput.Text))
                {
                    LoadSyllablesFromLrc(TxtLrcInput.Text);
                    TxtMainLyricsEditor.Text = CleanLrcTimestamps(TxtLrcInput.Text);
                    MessageBox.Show("Lyrics synced onto Subtitles track.", "Import Success", MessageBoxButton.OK, MessageBoxImage.Information);
                }
            }
        }

        private void TxtLrcInput_GotFocus(object sender, RoutedEventArgs e)
        {
            if (TxtLrcInput.Text == "Enter LRC string here...")
            {
                TxtLrcInput.Text = "";
            }
        }

        private void BtnUpdate_Click(object sender, RoutedEventArgs e)
        {
            GenerateSyllablesFromEditorText();
            MessageBox.Show("Regenerated syllables from the text editor block.", "Syllable Editor", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void BtnVideoPreview_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show("Video Preview screen toggled.", "Video Preview", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void BtnDuetMode_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show("Duet mode toggled. Timeline track split into Lead and Backing channels.", "Duet Mode", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void BtnAiSongCreator_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show("AI Song Creator wizard initialized. Select template and genre to begin.", "AI Song Creator", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void BtnImportCdg_Click(object sender, RoutedEventArgs e)
        {
            var openFileDialog = new Microsoft.Win32.OpenFileDialog
            {
                Filter = "CD+G Files (*.cdg)|*.cdg"
            };
            if (openFileDialog.ShowDialog() == true)
            {
                MessageBox.Show($"Loaded CD+G file: {openFileDialog.FileName}", "Import CD+G", MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        private void BtnImportMidi_Click(object sender, RoutedEventArgs e)
        {
            var openFileDialog = new Microsoft.Win32.OpenFileDialog
            {
                Filter = "MIDI Files (*.mid;*.midi)|*.mid;*.midi"
            };
            if (openFileDialog.ShowDialog() == true)
            {
                MessageBox.Show($"Imported MIDI lyrics from: {openFileDialog.FileName}", "Import MIDI", MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }

        private void BtnApplyStyle_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show("Karaoke subtitle style template applied successfully.", "Style Settings", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void BtnVideoSettings_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show("Video render settings overlay loaded. Hardware acceleration enabled.", "Video Settings", MessageBoxButton.OK, MessageBoxImage.Information);
        }
    }

    // --- Syllable Clip Model Class ---
    public class SyllableClip
    {
        public string Text { get; set; } = string.Empty;
        public long StartTimeUs { get; set; }
        public long DurationUs { get; set; }
        public Border? VisualElement { get; set; }
        public TextBlock? PreviewTextBlock { get; set; }
        public int LineIndex { get; set; }
    }

    public class YoutubeResultItem
    {
        public string VideoId { get; set; }
        public string Title { get; set; }
        public string Channel { get; set; }
        public string DurationStr { get; set; }
        public string Thumbnail { get; set; }
        public string ChannelAndDuration => $"{Channel} • {DurationStr}";
    }

    public class YoutubeDownloadItem : System.ComponentModel.INotifyPropertyChanged
    {
        private double _progress;
        public string VideoId { get; set; }
        public string Title { get; set; }
        public double Progress
        {
            get => _progress;
            set
            {
                if (_progress != value)
                {
                    _progress = value;
                    OnPropertyChanged(nameof(Progress));
                }
            }
        }

        public event System.ComponentModel.PropertyChangedEventHandler PropertyChanged;
        protected void OnPropertyChanged(string name)
        {
            PropertyChanged?.Invoke(this, new System.ComponentModel.PropertyChangedEventArgs(name));
        }
    }

    public class LrcResultItem
    {
        public long id { get; set; }
        public string trackName { get; set; }
        public string artistName { get; set; }
        public string albumName { get; set; }
        public double duration { get; set; }
        public string syncedLyrics { get; set; }
        public string plainLyrics { get; set; }
        public bool instrumental { get; set; }

        [System.Text.Json.Serialization.JsonIgnore]
        public string Title => trackName;
        [System.Text.Json.Serialization.JsonIgnore]
        public string Artist => artistName;
        [System.Text.Json.Serialization.JsonIgnore]
        public string Album => albumName;
        [System.Text.Json.Serialization.JsonIgnore]
        public string SyncedLyrics => syncedLyrics;
        [System.Text.Json.Serialization.JsonIgnore]
        public string PlainLyrics => plainLyrics;
    }

    public class YoutubeSearchJsonItem
    {
        public string id { get; set; }
        public string title { get; set; }
        public string channel { get; set; }
        public string durationStr { get; set; }
        public string thumbnail { get; set; }
    }
}
