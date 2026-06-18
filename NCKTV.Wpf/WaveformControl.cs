using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace NCKTV.Wpf
{
    public class WaveformControl : FrameworkElement
    {
        private static readonly Brush _defaultCenterBrush = new SolidColorBrush(Color.FromRgb(58, 58, 74));
        private static readonly Pen _defaultCenterPen;

        private static readonly Brush _verticalGradientBrush;
        private static readonly Brush _horizontalGradientBrush;
        private static readonly Pen _verticalPeakPen;
        private static readonly Pen _horizontalPeakPen;

        private static readonly Brush _baselineBrush = new SolidColorBrush(Color.FromArgb(80, 124, 77, 255));
        private static readonly Pen _baselinePen;

        private ScrollViewer? _parentScrollViewer;

        // Peak caching for UI thread smoothness
        private string _cachedSourceFile = string.Empty;
        private long _cachedSourceStart = -1;
        private long _cachedDuration = -1;
        private long _cachedVisibleStartUs = -1;
        private long _cachedVisibleDurationUs = -1;
        private int _cachedNumPoints = -1;
        private float[]? _cachedMinPeaks;
        private float[]? _cachedMaxPeaks;
        private int _cachedPointsRead = 0;

        static WaveformControl()
        {
            _defaultCenterBrush.Freeze();
            _defaultCenterPen = new Pen(_defaultCenterBrush, 1);
            _defaultCenterPen.Freeze();

            _baselineBrush.Freeze();
            _baselinePen = new Pen(_baselineBrush, 1);
            _baselinePen.Freeze();

            var vGrad = new LinearGradientBrush(
                Color.FromRgb(124, 77, 255),  // Electric Violet
                Color.FromArgb(120, 26, 26, 36), // Translucent charcoal center
                new Point(0, 0.5),
                new Point(1, 0.5)
            );
            vGrad.Freeze();
            _verticalGradientBrush = vGrad;

            var hGrad = new LinearGradientBrush(
                Color.FromRgb(124, 77, 255),  // Electric Violet
                Color.FromArgb(120, 26, 26, 36), // Translucent charcoal center
                new Point(0.5, 0),
                new Point(0.5, 1)
            );
            hGrad.Freeze();
            _horizontalGradientBrush = hGrad;

            var vPen = new Pen(vGrad, 2)
            {
                StartLineCap = PenLineCap.Round,
                EndLineCap = PenLineCap.Round
            };
            vPen.Freeze();
            _verticalPeakPen = vPen;

            var hPen = new Pen(hGrad, 2)
            {
                StartLineCap = PenLineCap.Round,
                EndLineCap = PenLineCap.Round
            };
            hPen.Freeze();
            _horizontalPeakPen = hPen;
        }

        public WaveformControl()
        {
            this.Loaded += WaveformControl_Loaded;
            this.Unloaded += WaveformControl_Unloaded;
        }

        private void WaveformControl_Loaded(object sender, RoutedEventArgs e)
        {
            _parentScrollViewer = GetParentScrollViewer(this);
            if (_parentScrollViewer != null)
            {
                _parentScrollViewer.ScrollChanged += ScrollViewer_ScrollChanged;
            }
        }

        private void WaveformControl_Unloaded(object sender, RoutedEventArgs e)
        {
            if (_parentScrollViewer != null)
            {
                _parentScrollViewer.ScrollChanged -= ScrollViewer_ScrollChanged;
                _parentScrollViewer = null;
            }
        }

        private void ScrollViewer_ScrollChanged(object sender, ScrollChangedEventArgs e)
        {
            if (Orientation == Orientation.Vertical && e.VerticalChange != 0)
            {
                InvalidateVisual();
            }
            else if (Orientation == Orientation.Horizontal && e.HorizontalChange != 0)
            {
                InvalidateVisual();
            }
        }

        private ScrollViewer? GetParentScrollViewer(DependencyObject child)
        {
            DependencyObject parentObject = VisualTreeHelper.GetParent(child);
            if (parentObject == null) return null;

            if (parentObject is ScrollViewer scrollViewer)
            {
                return scrollViewer;
            }
            else
            {
                return GetParentScrollViewer(parentObject);
            }
        }

        public static readonly DependencyProperty SourceFileProperty =
            DependencyProperty.Register(nameof(SourceFile), typeof(string), typeof(WaveformControl),
                new FrameworkPropertyMetadata(string.Empty, FrameworkPropertyMetadataOptions.AffectsRender));

        public static readonly DependencyProperty SourceStartProperty =
            DependencyProperty.Register(nameof(SourceStart), typeof(long), typeof(WaveformControl),
                new FrameworkPropertyMetadata(0L, FrameworkPropertyMetadataOptions.AffectsRender));

        public static readonly DependencyProperty DurationProperty =
            DependencyProperty.Register(nameof(Duration), typeof(long), typeof(WaveformControl),
                new FrameworkPropertyMetadata(0L, FrameworkPropertyMetadataOptions.AffectsRender));

        public static readonly DependencyProperty OrientationProperty =
            DependencyProperty.Register(nameof(Orientation), typeof(Orientation), typeof(WaveformControl),
                new FrameworkPropertyMetadata(Orientation.Horizontal, FrameworkPropertyMetadataOptions.AffectsRender));

        public string SourceFile
        {
            get => (string)GetValue(SourceFileProperty);
            set => SetValue(SourceFileProperty, value);
        }

        public long SourceStart
        {
            get => (long)GetValue(SourceStartProperty);
            set => SetValue(SourceStartProperty, value);
        }

        public long Duration
        {
            get => (long)GetValue(DurationProperty);
            set => SetValue(DurationProperty, value);
        }

        public Orientation Orientation
        {
            get => (Orientation)GetValue(OrientationProperty);
            set => SetValue(OrientationProperty, value);
        }

        protected override void OnRender(DrawingContext drawingContext)
        {
            base.OnRender(drawingContext);

            double w = ActualWidth;
            double h = ActualHeight;

            if (w <= 0 || h <= 0) return;

            // Render background
            drawingContext.DrawRectangle(Brushes.Transparent, null, new Rect(0, 0, w, h));

            bool isVertical = Orientation == Orientation.Vertical;

            if (string.IsNullOrEmpty(SourceFile) || Duration <= 0)
            {
                // Draw a flat center line if no file or duration is specified
                if (isVertical)
                    drawingContext.DrawLine(_defaultCenterPen, new Point(w / 2, 0), new Point(w / 2, h));
                else
                    drawingContext.DrawLine(_defaultCenterPen, new Point(0, h / 2), new Point(w, h / 2));
                return;
            }

            double lengthSize = isVertical ? h : w;

            // Viewport bounds calculation
            double visibleStart = 0;
            double visibleEnd = lengthSize;

            if (_parentScrollViewer != null)
            {
                if (isVertical)
                {
                    try
                    {
                        Point localTop = _parentScrollViewer.TranslatePoint(new Point(0, 0), this);
                        Point localBottom = _parentScrollViewer.TranslatePoint(new Point(0, _parentScrollViewer.ViewportHeight), this);
                        
                        visibleStart = Math.Max(0, localTop.Y);
                        visibleEnd = Math.Min(lengthSize, localBottom.Y);
                    }
                    catch
                    {
                        visibleStart = Math.Max(0, _parentScrollViewer.VerticalOffset);
                        visibleEnd = Math.Min(lengthSize, _parentScrollViewer.VerticalOffset + _parentScrollViewer.ViewportHeight);
                    }
                }
                else
                {
                    try
                    {
                        Point localLeft = _parentScrollViewer.TranslatePoint(new Point(0, 0), this);
                        Point localRight = _parentScrollViewer.TranslatePoint(new Point(_parentScrollViewer.ViewportWidth, 0), this);
                        
                        visibleStart = Math.Max(0, localLeft.X);
                        visibleEnd = Math.Min(lengthSize, localRight.X);
                    }
                    catch
                    {
                        visibleStart = Math.Max(0, _parentScrollViewer.HorizontalOffset);
                        visibleEnd = Math.Min(lengthSize, _parentScrollViewer.HorizontalOffset + _parentScrollViewer.ViewportWidth);
                    }
                }
            }

            double visibleSize = visibleEnd - visibleStart;
            if (visibleSize <= 5.0) return; // Viewport has no size or control is hidden

            double usPerPixel = Duration / lengthSize;
            long visibleStartUs = SourceStart + (long)(visibleStart * usPerPixel);
            long visibleDurationUs = (long)(visibleSize * usPerPixel);

            int numPoints = (int)Math.Max(10, visibleSize / 4); // 1 point every 4 pixels for spacing

            float[] minPeaks;
            float[] maxPeaks;
            int pointsRead;

            // Check cache to avoid calling native code and blocking the UI thread repeatedly during drags
            if (SourceFile == _cachedSourceFile &&
                SourceStart == _cachedSourceStart &&
                Duration == _cachedDuration &&
                visibleStartUs == _cachedVisibleStartUs &&
                visibleDurationUs == _cachedVisibleDurationUs &&
                numPoints == _cachedNumPoints &&
                _cachedMinPeaks != null &&
                _cachedMaxPeaks != null)
            {
                minPeaks = _cachedMinPeaks;
                maxPeaks = _cachedMaxPeaks;
                pointsRead = _cachedPointsRead;
            }
            else
            {
                minPeaks = new float[numPoints];
                maxPeaks = new float[numPoints];
                pointsRead = NativeBridge.ncktv_get_waveform_peaks(SourceFile, visibleStartUs, visibleDurationUs, numPoints, minPeaks, maxPeaks);

                // Update cache
                _cachedSourceFile = SourceFile;
                _cachedSourceStart = SourceStart;
                _cachedDuration = Duration;
                _cachedVisibleStartUs = visibleStartUs;
                _cachedVisibleDurationUs = visibleDurationUs;
                _cachedNumPoints = numPoints;
                _cachedMinPeaks = minPeaks;
                _cachedMaxPeaks = maxPeaks;
                _cachedPointsRead = pointsRead;
            }

            Pen peakPen = isVertical ? _verticalPeakPen : _horizontalPeakPen;

            double centerOffset = isVertical ? w / 2.0 : h / 2.0;
            double maxHalfSize = centerOffset * 0.9; // 10% padding

            // Draw center line only in the visible portion
            if (isVertical)
                drawingContext.DrawLine(_baselinePen, new Point(centerOffset, visibleStart), new Point(centerOffset, visibleEnd));
            else
                drawingContext.DrawLine(_baselinePen, new Point(visibleStart, centerOffset), new Point(visibleEnd, centerOffset));

            if (pointsRead <= 0)
            {
                // File might not be preloaded/decoded yet
                return;
            }

            double stepSize = visibleSize / numPoints;
            for (int i = 0; i < pointsRead; i++)
            {
                double pos = visibleStart + (i * stepSize) + 1.0;
                float minVal = minPeaks[i];
                float maxVal = maxPeaks[i];

                double offsetTop = centerOffset - (maxVal * maxHalfSize);
                double offsetBottom = centerOffset - (minVal * maxHalfSize);

                // Silence protection: draw a tiny bar if it's too small
                if (Math.Abs(offsetBottom - offsetTop) < 2.0)
                {
                    offsetTop = centerOffset - 1.0;
                    offsetBottom = centerOffset + 1.0;
                }

                if (isVertical)
                {
                    drawingContext.DrawLine(peakPen, new Point(offsetTop, pos), new Point(offsetBottom, pos));
                }
                else
                {
                    drawingContext.DrawLine(peakPen, new Point(pos, offsetTop), new Point(pos, offsetBottom));
                }
            }
        }
    }
}
