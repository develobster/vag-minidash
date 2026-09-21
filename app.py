"""Run with python app.py. Python standard library + Tk only."""
import math
import json
import time
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
from pathlib import Path
from core import Simulator, Coding, Recorder
from catalog import load_vehicle, load_catalog, assess, byte_diff, ROOT
from diagnostics import FakeECU, run_lab

root = tk.Tk()
root.title('VAG MiniDash • Development simulator')
root.geometry('980x680')
root.configure(bg='#10171e')
def open_diagnostic_lab():
    window = tk.Toplevel(root)
    window.title('Диагностическая лаборатория • только Fake ECU')
    window.geometry('840x600')
    frame = ttk.Frame(window, padding=16)
    frame.pack(fill='both', expand=True)
    ttk.Label(frame, text='Синтетический ЭБУ. Нет связи с автомобилем. Время ожидания виртуальное.').pack(anchor='w', pady=6)
    mode = tk.StringVar(value='normal')
    ttk.Combobox(frame, textvariable=mode, values=FakeECU.MODES, state='readonly').pack(fill='x')
    ttk.Label(frame, text='normal: норма · pending: задержка · silent: молчание · negative: отказ\nendless_pending: бесконечное ожидание · wrong_echo: чужой ответ · truncated: повреждение').pack(anchor='w', pady=6)
    output = tk.Text(frame, wrap='word', bg='#182530', fg='#e6eff6')
    output.pack(fill='both', expand=True, pady=10)
    last_result = None
    def run():
        nonlocal last_result
        last_result = run_lab(mode.get())
        output.delete('1.0', 'end')
        output.insert('end', json.dumps(last_result, ensure_ascii=False, indent=2))
    def save():
        if last_result is None:
            messagebox.showinfo('Лаборатория', 'Сначала запусти сценарий.', parent=window)
            return
        path = filedialog.asksaveasfilename(parent=window, defaultextension='.json', initialfile='fake-ecu-report.json')
        if path:
            try:
                Path(path).write_text(json.dumps(last_result, ensure_ascii=False, indent=2), encoding='utf8')
            except OSError as exc:
                messagebox.showerror('Не удалось сохранить', str(exc), parent=window)
    controls = ttk.Frame(frame)
    controls.pack(fill='x')
    ttk.Button(controls, text='Запустить сценарий', command=run).pack(side='left')
    ttk.Button(controls, text='Экспорт JSON', command=save).pack(side='left', padx=8)
    run()
menu = tk.Menu(root)
tools_menu = tk.Menu(menu, tearoff=False)
tools_menu.add_command(label='Диагностическая лаборатория (Fake ECU)', command=open_diagnostic_lab)
menu.add_cascade(label='Инструменты', menu=tools_menu)
root.configure(menu=menu)
style = ttk.Style()
style.theme_use('clam')
style.configure('.', background='#10171e', foreground='#e6eff6', font=('Arial', 12))
style.configure('TButton', padding=12, background='#263947')
style.configure('TNotebook.Tab', padding=(18, 12))
style.configure('Hero.TLabel', font=('Arial', 32, 'bold'), foreground='#61e5bc')
style.configure('TFrame', background='#10171e')
style.configure('TLabel', background='#10171e')
sim = Simulator()
engine = Coding(sim, Path.home() / 'VAG-MiniDash' / 'coding-journal.jsonl')
rec = Recorder()
last_undo = None
running = tk.BooleanVar(value=False)
start = time.monotonic()
ttk.Label(root, text='VAG / MINIDASH', style='Hero.TLabel').pack(anchor='w', padx=24, pady=(20, 4))
ttk.Label(root, text='Tiguan II · CZPA / DQ500     |     СИМУЛЯЦИЯ — связи с автомобилем нет').pack(anchor='w', padx=24, pady=8)
notebook = ttk.Notebook(root)
notebook.pack(fill='both', expand=True, padx=24, pady=20)
dash, coding, logs, about = [ttk.Frame(notebook, padding=20) for _ in range(4)]
for frame, title in zip((dash, coding, logs, about), ('Приборы', 'Кодирование', 'Чёрный ящик', 'Подключение')):
    notebook.add(frame, text=title)
metrics = {}
for i, (key, label) in enumerate([('speed','Скорость · км/ч'),('rpm','Обороты · об/мин'),('gear','Передача'),('oil','Масло двигателя · °C'),('dsg','Масло DSG · °C'),('voltage','Напряжение · В')]):
    frame = ttk.Frame(dash, padding=18)
    frame.grid(row=i//3, column=i%3, sticky='nsew')
    dash.columnconfigure(i%3, weight=1)
    ttk.Label(frame, text=label).pack(anchor='w')
    metrics[key] = ttk.Label(frame, text='—', style='Hero.TLabel')
    metrics[key].pack(anchor='w', pady=10)
def drive():
    running.set(not running.get())
    drive_button.configure(text='Остановить симуляцию' if running.get() else 'Начать поездку')
drive_button = ttk.Button(dash, text='Начать поездку', command=drive)
drive_button.grid(row=2, column=0, columnspan=3, pady=20)
ttk.Label(coding, text='Учебные функции: названия не являются рецептами кодирования VAG.', wraplength=820).pack(anchor='w', pady=10)
status = ttk.Label(coding, text='Выбери изменение; исходное значение сохранится в журнале.')
status.pack(anchor='w', pady=14)
buttons = []
def apply(key):
    global last_undo
    try:
        change = engine.prepare(key, not sim.read(key))
        if not messagebox.askokcancel('Предпросмотр · симулятор', f'{key}\n{change.before} → {change.after}\n\nИзменить значение в симуляторе?'):
            return
        last_undo = engine.apply(change)
        status.configure(text='Изменение записано в симулятор. Чтение подтвердило результат.')
        refresh()
    except Exception as e:
        messagebox.showerror('Изменение не выполнено', str(e))
def refresh():
    for key, label, button in buttons:
        button.configure(text=f'{label}: {"ВКЛ" if sim.read(key) else "ВЫКЛ"}')
for key, label in [('demo_door_light','Учебная подсветка двери'),('demo_mirror_fold','Учебное складывание зеркал')]:
    button = ttk.Button(coding, command=lambda k=key: apply(k))
    button.pack(fill='x', pady=8)
    buttons.append((key, label, button))
def undo():
    global last_undo
    if last_undo is None:
        return
    if not messagebox.askokcancel('Возврат · симулятор', 'Вернуть последнее исходное значение?'):
        return
    try:
        engine.apply(last_undo)
        last_undo = None
        refresh()
        status.configure(text='Исходное значение восстановлено и проверено.')
    except Exception as e:
        messagebox.showerror('Возврат не выполнен', str(e))
ttk.Button(coding, text='Вернуть последнее изменение', command=undo).pack(fill='x', pady=8)
refresh()
catalog_tab = ttk.Frame(notebook, padding=12)
notebook.add(catalog_tab, text='Каталог VAG')
vehicle = load_vehicle(ROOT / 'profiles/universal.json')
features = load_catalog()
vehicle_label = ttk.Label(catalog_tab, text=vehicle['name'])
vehicle_label.pack(anchor='w')
ttk.Label(catalog_tab, text='Офлайн-каталог: импорт JSON не подключает автомобиль и не разрешает запись.').pack(anchor='w', pady=6)
tree = ttk.Treeview(catalog_tab, columns=('category', 'status'), height=7)
tree.heading('#0', text='Функция')
tree.heading('category', text='Раздел')
tree.heading('status', text='Статус')
tree.column('#0', width=350)
tree.column('category', width=100)
tree.column('status', width=150)
tree.pack(fill='both', expand=True)
details = tk.Text(catalog_tab, height=5, wrap='word', bg='#182530', fg='#e6eff6')
details.pack(fill='x', pady=6)
def show_feature(event=None):
    selected = tree.selection()
    if not selected:
        return
    feature = next(f for f in features if f['id'] == selected[0])
    result = assess(feature, vehicle)
    details.configure(state='normal')
    details.delete('1.0', 'end')
    details.insert('end', feature['requirements'] + '\n' + '\n'.join(result['reasons']) + '\n' + feature['evidence'])
    details.configure(state='disabled')
for feature in features:
    tree.insert('', 'end', iid=feature['id'], text=feature['title'],
                values=(feature['category'], 'Исключено' if feature['scope']=='excluded' else 'Исследование'))
tree.bind('<<TreeviewSelect>>', show_feature)
def import_inventory():
    global vehicle
    path = filedialog.askopenfilename(filetypes=[('Vehicle inventory', '*.json')])
    if not path:
        return
    try:
        imported = load_vehicle(path)
        vehicle = imported
        vehicle_label.configure(text=vehicle['name'] + ' | импорт, не живая идентификация')
        show_feature()
    except (ValueError, OSError) as exc:
        messagebox.showerror('Неверный профиль', str(exc))
ttk.Button(catalog_tab, text='Импортировать инвентаризацию JSON', command=import_inventory).pack(anchor='w')
diff_tab = ttk.Frame(notebook, padding=16)
notebook.add(diff_tab, text='Сравнение HEX')
ttk.Label(diff_tab, text='Только офлайн-сравнение. Байты и биты с нуля. Команды в автомобиль не отправляются.').pack(anchor='w')
before_entry, after_entry = ttk.Entry(diff_tab), ttk.Entry(diff_tab)
for title, entry in [('Исходная строка HEX', before_entry), ('Новая строка HEX', after_entry)]:
    ttk.Label(diff_tab, text=title).pack(anchor='w', pady=6)
    entry.pack(fill='x')
diff_output = tk.Text(diff_tab, height=9, bg='#182530', fg='#e6eff6')
diff_output.pack(fill='both', expand=True, pady=10)
def compare_hex():
    try:
        changes = byte_diff(before_entry.get(), after_entry.get())
        diff_output.delete('1.0', 'end')
        diff_output.insert('end', '\n'.join(f"Байт {r['byte']}: {r['before']} → {r['after']}; биты {r['changed_bits_lsb0']}" for r in changes) or 'Различий нет')
    except ValueError as exc:
        messagebox.showerror('Неверные HEX-данные', str(exc))
ttk.Button(diff_tab, text='Сравнить', command=compare_hex).pack(anchor='w')
log_status = ttk.Label(logs, text='')
log_status.pack(anchor='w', pady=20)
ttk.Label(logs, text='Сохраняются последние 30 секунд и события D2→D1 с 5 секундами после переключения.\nЭто фиксация перехода передачи, а не автоматический диагноз неисправности.', wraplength=800).pack(anchor='w')
def export():
    path = filedialog.asksaveasfilename(defaultextension='.json', initialfile='minidash-session.json')
    if path:
        rec.export(path)
        messagebox.showinfo('Экспорт', 'Данные симуляции сохранены.')
ttk.Button(logs, text='Экспортировать JSON', command=export).pack(anchor='w', pady=20)
ttk.Label(about, text='Текущий транспорт: SIMULATOR\n\nРеальный CAN, ISO-TP/UDS и VNCI пока не подключены.\nПрофиль CZPA/DQ500 — шаблон, реальные DID и CAN-ID не заполнены.\n\nЦелевая архитектура V1: Windows/Linux-приложение → диагностический адаптер → автомобиль.\nАвтономная V2: ESP32-S3 → внешний CAN-трансивер → небольшой SPI/I²C OLED.\nКодирование: отдельный сервис с привязкой к HW/SW блока.\n\nПрошивка ЭБУ и базовые установки DSG в этот прототип не входят.', wraplength=810).pack(anchor='w')
def tick():
    phase = (time.monotonic()-start)%24
    speed = max(0, round(60*(1-abs(phase-12)/12))) if running.get() else 0
    gear = 1 if speed<16 else 2 if speed<30 else 3 if speed<46 else 4
    sim.speed = speed
    data = dict(time=time.time(), speed=speed, rpm=round(800+speed*25), gear=gear, oil=96, dsg=72, voltage=sim.voltage)
    rec.feed(data)
    for key, widget in metrics.items():
        widget.configure(text=str(data[key]))
    log_status.configure(text=f'В буфере: {len(rec.ring)} отсчётов · Событий: {len(rec.events)} · Запись после события: {rec.remaining if rec.pending else 0}')
    root.after(100, tick)
tick()
root.mainloop()
