import requests
import gi
import sys
import cairo
from datetime import datetime
import signal

# Initialize GTK and Pango
gi.require_version('Gtk', '3.0')
gi.require_version('Pango', '1.0')
from gi.repository import Gtk, Gdk, GLib, Pango, GdkPixbuf

class AStockTicker(Gtk.Window):
    def __init__(self, code, opacity=0.8):
        super().__init__()
        self.data = None
        self.code = code  # Stock code from command line argument
        self.opacity = opacity  # Transparency level (0.0-1.0)
        self.price_color = Gdk.RGBA(1, 1, 1, 1)  # Default white color
        
        # Basic window settings - reduced window size
        self.set_decorated(False)  # Remove all window decorations
        self.set_size_request(100, 30)  # Reduce window dimensions
        self.set_resizable(False)
        self.set_keep_above(True)  # Keep window above others
        self.set_skip_taskbar_hint(True)
        
        # Set window transparency properties
        self.set_visual(Gdk.Screen.get_rgba_visual(Gdk.Screen.get_default()))
        self.set_app_paintable(True)  # Allow custom background painting
        
        # Drag-related variables
        self.drag_enabled = False
        self.drag_start_x = 0
        self.drag_start_y = 0
        
        # Load CSS styles
        self.load_css()
        
        # Create UI elements
        self.create_widgets()
        
        # Fetch initial data
        self.update_data()
        
        # Set up periodic refresh (every 60 seconds)
        self.timeout_id = GLib.timeout_add(60000, self.update_data)
        
        # Connect signals
        self.connect("destroy", Gtk.main_quit)
        self.connect("button-press-event", self.on_button_press)
        self.connect("motion-notify-event", self.on_motion_notify)
        self.connect("draw", self.on_draw)  # For transparent background
        
        # Create right-click menu
        self.create_context_menu()
    
    def load_css(self):
        """Load CSS styles to remove default margins and background"""
        css = """
        window {
            background-color: rgba(0, 0, 0, 0); 
            /* Fully transparent background */
            border: none; 
            /* Remove border */
            padding: 0;    
            /* Remove padding */
        }
        .main-box {
            background-color: rgba(0, 0, 0, 0.7); 
            /* Semi-transparent black background */
            border-radius: 4px; 
            /* Slight rounded corners */
            padding: 1px;       
            /* Reduced padding */
        }
        """
        css_provider = Gtk.CssProvider()
        css_provider.load_from_data(css.encode('utf-8'))
        style_context = self.get_style_context()
        style_context.add_provider_for_screen(
            Gdk.Screen.get_default(),
            css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )
    
    def create_widgets(self):
        # Main layout
        main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)  # Reduced spacing
        main_box.set_name("main-box")
        main_box.set_border_width(0)
        self.add(main_box)
        
        # Stock price (adjusted to similar size as change percentage)
        self.price_label = Gtk.Label(label="--")
        self.price_label.override_font(Pango.FontDescription("SimHei Bold 10"))  # Reduced font size
        self.price_label.override_color(Gtk.StateFlags.NORMAL, self.price_color)
        main_box.pack_start(self.price_label, True, True, 0)
        
        # Price change percentage
        self.change_label = Gtk.Label(label="--")
        self.change_label.override_font(Pango.FontDescription("SimHei 8"))  # Keep slightly smaller
        self.change_label.override_color(Gtk.StateFlags.NORMAL, self.price_color)
        main_box.pack_start(self.change_label, True, True, 0)
    
    def on_draw(self, widget, cr):
        """Draw transparent background"""
        cr.set_source_rgba(0, 0, 0, 0)  # Transparent
        cr.set_operator(cairo.OPERATOR_CLEAR)
        cr.paint()
        cr.set_operator(cairo.OPERATOR_OVER)
        return False
    
    def create_context_menu(self):
        self.menu = Gtk.Menu()
        quit_item = Gtk.MenuItem(label="Quit")
        quit_item.connect("activate", lambda _: Gtk.main_quit())
        refresh_item = Gtk.MenuItem(label="Refresh")
        refresh_item.connect("activate", lambda _: self.update_data())
        opacity_item = Gtk.MenuItem(label="Adjust Opacity")
        opacity_item.connect("activate", self.adjust_opacity)
        self.menu.append(quit_item)
        self.menu.append(refresh_item)
        self.menu.append(opacity_item)
        self.menu.show_all()
    
    def adjust_opacity(self, widget):
        """Opacity adjustment dialog"""
        dialog = Gtk.Dialog(title="Adjust Opacity", transient_for=self, flags=0)
        dialog.add_buttons(Gtk.STOCK_OK, Gtk.ResponseType.OK)
        scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0.3, 1.0, 0.1)
        scale.set_value(self.opacity)
        scale.connect("value-changed", lambda s: setattr(self, 'opacity', s.get_value()))
        dialog.get_content_area().pack_start(scale, True, True, 0)
        dialog.show_all()
        dialog.run()
        dialog.destroy()
        self.load_css()
    
    def on_button_press(self, widget, event):
        if event.button == 1:  # Left click for dragging
            self.drag_enabled = True
            self.drag_start_x = event.x_root
            self.drag_start_y = event.y_root
            return True
        elif event.button == 3:  # Right click for menu
            self.menu.popup_at_pointer(event)
            return True
        return False
    
    def on_motion_notify(self, widget, event):
        if self.drag_enabled and event.state & Gdk.ModifierType.BUTTON1_MASK:
            x, y = self.get_position()
            dx = event.x_root - self.drag_start_x
            dy = event.y_root - self.drag_start_y
            self.move(x + dx, y + dy)
            self.drag_start_x = event.x_root
            self.drag_start_y = event.y_root
            return True
        elif event.type == Gdk.EventType.BUTTON_RELEASE:
            self.drag_enabled = False
        return False
    
    def is_trading_time(self):
        """Check if current time is within stock market trading hours"""
        now = datetime.now()
        # Not trading on weekends
        if now.weekday() >= 5:
            return False
        
        hour, minute = now.hour, now.minute
        # Morning session: 9:15-11:30, but last update should after 11:30, so delay 2 minutes
        morning = (hour == 9 and minute >= 15) or (10 <= hour < 11) or (hour == 11 and minute <= 32)
        # Afternoon session: 13:00-15:00, but last update should after 15:00, so delay 2 minutes
        afternoon = (13 <= hour < 15) or (hour == 15 and minute <= 2)
        
        return morning or afternoon

    def update_data(self):
        """Update stock data, refresh display"""
        # Only fetch new data during trading hours or if no data exists
        if self.is_trading_time() or self.data is None:
            self.data = get_sina_stock_price(self.code)
        
        if self.data and len(self.data) > 4:
            try:
                price0 = float(self.data[2])  # Yesterday's closing price
                price = float(self.data[3])   # Current price
                change = price - price0
                change_percent = (change / price0) * 100 if price0 != 0 else 0
                
                # Set color based on price movement, apply to both labels
                if change >= 0:
                    # Price up - red
                    self.price_color = Gdk.RGBA(1, 0.3, 0.3, 1)
                    self.change_label.set_text(f"+{change:.2f} (+{change_percent:.2f}%)")
                else:
                    # Price down - green
                    self.price_color = Gdk.RGBA(0.3, 1, 0.3, 1)
                    self.change_label.set_text(f"{change:.2f} ({change_percent:.2f}%)")
                
                # Update price label (apply color and text)
                self.price_label.set_text(f"{price:.2f}")
                self.price_label.override_color(Gtk.StateFlags.NORMAL, self.price_color)
                self.change_label.override_color(Gtk.StateFlags.NORMAL, self.price_color)
                
            except (ValueError, IndexError) as e:
                print(f"Parsing error: {e}")
        
        # Return True to keep the timeout active
        return True

def get_sina_stock_price(stock_code):
    """Fetch stock price from Sina Finance API"""
    url = f'http://hq.sinajs.cn/list={stock_code}'
    headers = {
        'Referer': 'https://finance.sina.com.cn/',
        'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
    }
    try:
        response = requests.get(url, headers=headers)
        response.encoding = 'gbk'
        if 'var hq_str_' in response.text:
            # Extract and split the data string
            return response.text.split('"')[1].split(',')
    except Exception as e:
        print(f"Request failed: {e}")
    return None

if __name__ == "__main__":
    # Check if stock code is provided as command line argument
    if len(sys.argv) != 2:
        print("Usage: python stock_ticker.py <stock_code>")
        print("Example: python stock_ticker.py sh000001")
        sys.exit(1)
    
    # Handle Ctrl+C to exit gracefully
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    
    # Get stock code from command line argument
    target_stock = sys.argv[1]
    app = AStockTicker(target_stock)
    app.show_all()
    Gtk.main()
    
