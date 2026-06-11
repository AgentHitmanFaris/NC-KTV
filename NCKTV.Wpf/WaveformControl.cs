using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace NCKTV.Wpf
{
    public class WaveformControl : FrameworkElement
    {
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
                var centerPen = new Pen(new SolidColorBrush(Color.FromRgb(58, 58, 74)), 1);
                if (isVertical)
                    drawingContext.DrawLine(centerPen, new Point(w / 2, 0), new Point(w / 2, h));
                else
                    drawingContext.DrawLine(centerPen, new Point(0, h / 2), new Point(w, h / 2));
                return;
            }

            double lengthSize = isVertical ? h : w;
            int numPoints = (int)Math.Max(10, lengthSize / 4); // 1 point every 4 pixels for spacing
            float[] minPeaks = new float[numPoints];
            float[] maxPeaks = new float[numPoints];

            int pointsRead = NativeBridge.ncktv_get_waveform_peaks(SourceFile, SourceStart, Duration, numPoints, minPeaks, maxPeaks);

            var gradient = new LinearGradientBrush(
                Color.FromRgb(124, 77, 255),  // Electric Violet
                Color.FromArgb(120, 26, 26, 36), // Translucent charcoal center
                isVertical ? new Point(0, 0.5) : new Point(0.5, 0),
                isVertical ? new Point(1, 0.5) : new Point(0.5, 1)
            );

            var peakPen = new Pen(gradient, 2)
            {
                StartLineCap = PenLineCap.Round,
                EndLineCap = PenLineCap.Round
            };

            double centerOffset = isVertical ? w / 2.0 : h / 2.0;
            double maxHalfSize = centerOffset * 0.9; // 10% padding

            // Draw center line
            var baselinePen = new Pen(new SolidColorBrush(Color.FromArgb(80, 124, 77, 255)), 1);
            if (isVertical)
                drawingContext.DrawLine(baselinePen, new Point(centerOffset, 0), new Point(centerOffset, h));
            else
                drawingContext.DrawLine(baselinePen, new Point(0, centerOffset), new Point(w, centerOffset));

            if (pointsRead <= 0)
            {
                // File might not be preloaded/decoded yet
                return;
            }

            double stepSize = lengthSize / numPoints;
            for (int i = 0; i < pointsRead; i++)
            {
                double pos = i * stepSize + 1.0;
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
