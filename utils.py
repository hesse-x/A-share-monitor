import requests
from datetime import datetime


def is_trading_time(now=None):
    """Check if current time is within stock market trading hours"""
    now = datetime.now()
    # Not trading on weekends
    if now.weekday() >= 5:
        return False

    hour, minute = now.hour, now.minute
    # Morning session: 9:30-11:30, but last update should after 11:30, so delay 2 minutes
    morning = (hour == 9
               and minute >= 30) or (10 <= hour < 11) or (hour == 11
                                                          and minute <= 32)
    # Afternoon session: 13:00-15:00, but last update should after 15:00, so delay 2 minutes
    afternoon = (13 <= hour < 15) or (hour == 15 and minute <= 2)

    return morning or afternoon


def get_sina_stock_price(stock_code):
    """Fetch stock price from Sina Finance API"""
    url = f'http://hq.sinajs.cn/list={stock_code}'
    headers = {
        'Referer':
        'https://finance.sina.com.cn/',
        'User-Agent':
        'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
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
