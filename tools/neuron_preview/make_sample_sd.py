"""Writes sample /neuron files into ./sd for the preview (illustrative data)."""
import os
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SD = os.path.join(HERE, 'sd', 'neuron')
os.makedirs(os.path.join(SD, 'cache'), exist_ok=True)
now = int(time.time())


def write(rel, text):
    with open(os.path.join(SD, rel), 'w', encoding='utf-8') as f:
        f.write(text)


day = time.strftime('%Y%m%d')
write('habits.txt', f'workout|{day}|1101110111011101\nsteps|{day}|0111101111011110\n')
write('cache/weather.txt',
      f'updated={now - 25 * 60}\nplace=Pune\nnow=27.4,30.1,74,9.4,2\n'
      'd0=2026-10-05,2,30.8,21.9,35,06:24,18:18\n'
      'd1=2026-10-06,61,29.1,21.5,70,06:24,18:17\n'
      'd2=2026-10-07,3,29.9,21.2,40,06:25,18:16\n'
      'd3=2026-10-08,0,31.2,20.8,5,06:25,18:15\n')


def series(start, drift, n=22):
    import math
    return [round(start * (1 + drift * i / n + 0.012 * math.sin(i * 0.9)), 2) for i in range(n)]


rows = []
for ticker, label, base, drift in [('^NSEI', 'NIFTY 50', 25000, 0.03), ('^BSESN', 'SENSEX', 82000, 0.028),
                                   ('^NSEBANK', 'BANK NIFTY', 56000, -0.01), ('^CNXIT', 'NIFTY IT', 36000, -0.04),
                                   ('^INDIAVIX', 'INDIA VIX', 12, 0.08)]:
    s = series(base, drift)
    rows.append(f'{ticker}|{label}|{s[-1]:.2f}|{s[-2]:.2f}|' + ','.join(f'{v:.2f}' for v in s))
write('cache/markets.txt', f'updated={now - 15 * 60}\n' + '\n'.join(rows) + '\n')
write('cache/focus.txt', time.strftime('%Y%m%d') + '|3|75\n')
print('sample sd written')
