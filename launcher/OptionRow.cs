using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace StuntmanLauncher;

// One setting: label on the left, "< value >" on the right. Click the arrows,
// or focus the row and use Left/Right. Shows its hint in the window's footer
// while hovered or focused.
public sealed class OptionRow : Border
{
    private readonly TextBlock value_;
    private readonly Button prev_;
    private readonly Button next_;
    private readonly TextBlock label_;
    private List<(string Label, object Value)> options_ = new();
    private int index_;

    public string Hint { get; }
    public event Action<OptionRow>? Changed;
    public event Action<string>? HintShown;

    public OptionRow(string label, string hint)
    {
        Hint = hint;
        Focusable = true;
        FocusVisualStyle = null;
        Padding = new Thickness(14, 0, 6, 0);
        Height = 46;
        Margin = new Thickness(0, 0, 0, 4);
        Background = Brushes.Transparent;
        BorderThickness = new Thickness(3, 0, 0, 0);
        BorderBrush = Brushes.Transparent;
        Cursor = Cursors.Hand;

        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(250) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        label_ = new TextBlock
        {
            Text = label,
            VerticalAlignment = VerticalAlignment.Center,
            Style = (Style)Application.Current.Resources["RowLabel"],
        };
        prev_ = MakeArrow("", -1);
        value_ = new TextBlock
        {
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center,
            TextTrimming = TextTrimming.CharacterEllipsis,
            Style = (Style)Application.Current.Resources["RowValue"],
        };
        next_ = MakeArrow("", +1);

        Grid.SetColumn(label_, 0);
        Grid.SetColumn(prev_, 1);
        Grid.SetColumn(value_, 2);
        Grid.SetColumn(next_, 3);
        grid.Children.Add(label_);
        grid.Children.Add(prev_);
        grid.Children.Add(value_);
        grid.Children.Add(next_);
        Child = grid;

        MouseEnter += (_, _) => { UpdateHighlight(); HintShown?.Invoke(Hint); };
        MouseLeave += (_, _) => UpdateHighlight();
        GotKeyboardFocus += (_, _) => { UpdateHighlight(); HintShown?.Invoke(Hint); };
        LostKeyboardFocus += (_, _) => UpdateHighlight();
        MouseLeftButtonDown += (_, _) => Focus();
        IsEnabledChanged += (_, _) => Opacity = IsEnabled ? 1.0 : 0.4;
    }

    private Button MakeArrow(string glyph, int step)
    {
        var b = new Button
        {
            Content = glyph,
            Style = (Style)Application.Current.Resources["ArrowButton"],
            Focusable = false,
        };
        b.Click += (_, _) => { Focus(); Step(step); };
        return b;
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        if (e.Key == Key.Left) { Step(-1); e.Handled = true; }
        else if (e.Key == Key.Right || e.Key == Key.Enter || e.Key == Key.Space) { Step(+1); e.Handled = true; }
        base.OnKeyDown(e);
    }

    private void UpdateHighlight()
    {
        bool active = IsKeyboardFocused || IsMouseOver;
        Background = active ? (Brush)Application.Current.Resources["RowActiveBrush"] : Brushes.Transparent;
        BorderBrush = IsKeyboardFocused ? (Brush)Application.Current.Resources["AccentBrush"] : Brushes.Transparent;
    }

    public void SetOptions(IEnumerable<(string Label, object Value)> options, object selected)
    {
        options_ = options.ToList();
        index_ = Math.Max(0, options_.FindIndex(o => Equals(o.Value, selected)));
        Refresh();
    }

    public object Value => options_[index_].Value;

    public void Select(object value)
    {
        int i = options_.FindIndex(o => Equals(o.Value, value));
        if (i >= 0) { index_ = i; Refresh(); }
    }

    private void Step(int delta)
    {
        if (options_.Count < 2 || !IsEnabled) return;
        index_ = (index_ + delta + options_.Count) % options_.Count;
        Refresh();
        Changed?.Invoke(this);
    }

    private void Refresh()
    {
        value_.Text = options_.Count > 0 ? options_[index_].Label : "";
        bool many = options_.Count > 1;
        prev_.Visibility = next_.Visibility = many ? Visibility.Visible : Visibility.Hidden;
    }
}
