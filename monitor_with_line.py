import sys
import cairo
import time
import random  # 仅用于演示

import utils
from datetime import datetime

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk, GLib

max_points = 256
def find_pos(now):
    hour, minute = now.hour, now.minute
    if (hour <= 11):
        return (hour - 9) * 60 + min(minute, 30)
    return (hour - 13) * 60 + 121

def init_data(data, now, stock_code):
    info = utils.get_sina_stock_price(stock_code)
    if (utils.is_trading_time(now)):
        pos = find_pos(now)
        data[:] = [float(info[2])] * (pos - 1)
    data.append(float(info[3]))
    return float(info[2])

def append_data(data, now, stock_code):
    if (utils.is_trading_time(now)):
        info = utils.get_sina_stock_price(stock_code)
        pos = find_pos(now)
        data_size = len(data)
        shoule_size = pos - 1
        if (data_size > shoule_size):
            data[:] = data[:shoule_size]
        else:
            need = shoule_size - data_size
            data.extend([0] * need)
        data.append(float(info[3]))
    
def generate_data(data, baseline, stock_code):
    now = datetime.now()

    if (now.hour == 9 and now.minute >15 and now.minute < 20):
        data.clear()

    if (len(data) == 0):
        return init_data(data, now, stock_code)
    else:
        append_data(data, now, stock_code)
        return baseline


class DataMonitor(Gtk.Window):
    def __init__(self, stock_code):
        super().__init__(title="数据监控器 - 两位小数显示")
        self.stock_code = stock_code
        
        # 存储基线值
        self.baseline = 900
        
        # 数据初始化
        self.data = []
 
        # 减小文本区域宽度，让折线更靠左
        self.text_area_width = 45
        
        # 尺寸设置
        width=280
        height=100
        self.set_size_request(width, height)
        self.set_default_size(width, height)
        self.set_resizable(False)
        
        # 窗口其他设置 - 无边框
        self.set_decorated(False)
        self.set_skip_taskbar_hint(False)
        self.set_keep_above(True)
        self.connect("destroy", Gtk.main_quit)
        
        # 透明背景设置
        self.override_background_color(Gtk.StateType.NORMAL, Gdk.RGBA(0, 0, 0, 0))
        self.set_app_paintable(True)
        
        # 透明支持检测
        screen = self.get_screen()
        visual = screen.get_rgba_visual()
        if visual and screen.is_composited():
            self.set_visual(visual)
        else:
            print("警告：系统不支持透明背景")
        
        # 窗口拖动功能
        self.dragging = False
        self.drag_start_x = 0
        self.drag_start_y = 0
        self.connect("button-press-event", self.on_button_press)
        self.connect("button-release-event", self.on_button_release)
        self.connect("motion-notify-event", self.on_motion_notify)
       
        # 绘图区域
        self.drawing_area = Gtk.DrawingArea()
        self.drawing_area.override_background_color(Gtk.StateType.NORMAL, Gdk.RGBA(0, 0, 0, 0))
        self.drawing_area.set_hexpand(True)
        self.drawing_area.set_vexpand(True)
        self.drawing_area.connect("draw", self.on_draw)
        self.drawing_area.set_events(Gdk.EventMask.BUTTON_PRESS_MASK | 
                                    Gdk.EventMask.BUTTON_RELEASE_MASK | 
                                    Gdk.EventMask.POINTER_MOTION_MASK)
        self.add(self.drawing_area)
        
        # 启动数据更新定时器(替代多线程)
        self.running = True
        # 每1000毫秒(1秒)调用一次update_data，返回True保持定时器活跃
        self.timeout_id = GLib.timeout_add(1000, self.update_data)
        
        self.show_all()
    
    # 窗口拖动方法
    def on_button_press(self, widget, event):
        if event.button == 1:
            self.dragging = True
            self.drag_start_x = event.x_root
            self.drag_start_y = event.y_root
        elif event.button == 3:
            Gtk.main_quit()
        return True
    
    def on_button_release(self, widget, event):
        if event.button == 1:
            self.dragging = False
        return True
    
    def on_motion_notify(self, widget, event):
        if self.dragging:
            dx = event.x_root - self.drag_start_x
            dy = event.y_root - self.drag_start_y
            x, y = self.get_position()
            self.move(x + dx, y + dy)
            self.drag_start_x = event.x_root
            self.drag_start_y = event.y_root
        return True
    
    # 数据更新方法(定时器回调)
    def update_data(self):
        if not self.running:
            return False  # 返回False停止定时器
        
        # 生成新数据
        self.baseline = generate_data(self.data, self.baseline, self.stock_code)
        
        # 触发绘图更新
        self.drawing_area.queue_draw()
        return True  # 返回True保持定时器继续运行
    
    # 绘图逻辑 - 显示两位小数
    def on_draw(self, widget, cr):
        width = widget.get_allocated_width()
        height = widget.get_allocated_height()
        
        # 透明背景
        cr.set_operator(cairo.OPERATOR_CLEAR)
        cr.rectangle(0, 0, width, height)
        cr.fill()
        cr.set_operator(cairo.OPERATOR_OVER)
        
        data_count = len(self.data)
        if data_count < 1:
            return
        
        # 折线起点左移：绘图区域从减小后的文本区域宽度开始
        plot_width = width - self.text_area_width
        plot_start_x = self.text_area_width
        
        # 计算坐标映射参数
        x_unit = plot_width / max_points
        min_val, max_val = min(self.data + [self.baseline]), max(self.data + [self.baseline])
        padding = (max_val - min_val) * 0.1 if max_val > min_val else 10
        val_range = (max_val - min_val) + 2 * padding if max_val > min_val else 1
        
        # 计算所有点的坐标
        points = []
        for i in range(data_count):
            x = plot_start_x + i * x_unit
            y = height - (((self.data[i] - (min_val - padding)) / val_range) * height)
            points.append((x, y))
        
        # 计算基线的Y坐标
        baseline_y = height / 2
        
        # 绘制区域
        self.draw_region(cr, points, baseline_y, True, plot_width, height, plot_start_x)
        self.draw_region(cr, points, baseline_y, False, plot_width, height, plot_start_x)
        
        # 显示随动数据（两位小数）
        if self.data:
            last_value = self.data[-1]
            diff = last_value - self.baseline
            percentage = (diff / self.baseline) * 100 if self.baseline != 0 else 0
            percentage = max(min(percentage, 100), -100)
            
            # 设置颜色
            if last_value > self.baseline:
                cr.set_source_rgba(0.9, 0.4, 0.4, 0.8)
            else:
                cr.set_source_rgba(0.2, 0.7, 0.2, 0.8)
            
            # 字体设置
            cr.select_font_face("Arial", cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
            cr.set_font_size(10.5)
            
            # 格式化文本（两位小数）
            value_text = f"{last_value:.2f}"
            diff_text = f"{diff:+.2f}"
            percentage_text = f"{percentage:+.2f}%"
            
            # 计算文本高度
            _, _, _, value_height, _, _ = cr.text_extents(value_text)
            _, _, _, diff_height, _, _ = cr.text_extents(diff_text)
            
            # 文本起始位置
            margin = 2
            text_x = margin
            
            # 绘制数值
            value_y = margin + value_height
            cr.move_to(text_x, value_y)
            cr.show_text(value_text)
            
            # 绘制差值
            diff_y = value_y + diff_height + margin
            cr.move_to(text_x, diff_y)
            cr.show_text(diff_text)
            
            # 绘制百分比
            percent_y = diff_y + diff_height + margin
            cr.move_to(text_x, percent_y)
            cr.show_text(percentage_text)
            
            cr.stroke()
    
    def draw_region(self, cr, points, baseline_y, is_above, width, height, plot_start_x):
        """绘制区域渐变填充"""
        cr.new_path()
        path_started = False
        cross_baseline = False
        
        for i, (x, y) in enumerate(points):
            in_region = (y <= baseline_y) if is_above else (y >= baseline_y)
            
            if in_region:
                if not path_started:
                    cr.move_to(x, y)
                    path_started = True
                else:
                    cr.line_to(x, y)
            
            if i > 0:
                prev_x, prev_y = points[i-1]
                prev_in = (prev_y <= baseline_y) if is_above else (prev_y >= baseline_y)
                
                if in_region != prev_in:
                    t = (baseline_y - prev_y) / (y - prev_y) if (y - prev_y) != 0 else 0
                    cross_x = prev_x + t * (x - prev_x)
                    
                    if in_region:
                        cr.move_to(cross_x, baseline_y)
                        cr.line_to(x, y)
                        path_started = True
                    else:
                        cr.line_to(cross_x, baseline_y)
                        cross_baseline = True
        
        if path_started:
            if cross_baseline:
                cr.close_path()
            else:
                last_x = points[-1][0]
                if is_above:
                    cr.line_to(last_x, baseline_y)
                    cr.line_to(points[0][0], baseline_y)
                else:
                    cr.line_to(last_x, height)
                    cr.line_to(points[0][0], height)
                cr.close_path()
            
            # 渐变设置
            if is_above:
                gradient = cairo.LinearGradient(0, 0, 0, baseline_y)
                gradient.add_color_stop_rgba(0, 1, 0, 0, 0.8)
                gradient.add_color_stop_rgba(1, 1, 0, 0, 0)
            else:
                gradient = cairo.LinearGradient(0, baseline_y, 0, height)
                gradient.add_color_stop_rgba(0, 0, 1, 0, 0)
                gradient.add_color_stop_rgba(1, 0, 1, 0, 0.8)
            
            cr.set_source(gradient)
            cr.fill()

    def stop(self):
        self.running = False
        # 移除定时器
        if hasattr(self, 'timeout_id'):
            GLib.source_remove(self.timeout_id)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python stock_ticker.py <stock_code>")
        print("Example: python stock_ticker.py sh000001")
        sys.exit(1)

    target_stock = sys.argv[1]
    app = DataMonitor(target_stock)
    try:
        Gtk.main()
    except KeyboardInterrupt:
        app.stop()
