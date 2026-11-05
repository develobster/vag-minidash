"""Generate labelled net illustrations, not guessed physical PCB footprints."""
from pathlib import Path
from html import escape
import csv

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/review-2026-11-05/wiring'
C = dict(gnd='#354457', v5='#d23b37', v33='#d77511', tx='#7b46c5', rx='#007ea7', h='#1a8651', l='#aa6b16', spi='#3263ac')

class Sheet:
    def __init__(self, title, sub):
        self.p = ['<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="1100" viewBox="0 0 1600 1100">',
                  '<rect width="1600" height="1100" fill="#f5f7fb"/>',
                  '<style>text{font-family:DejaVu Sans,sans-serif;fill:#18273b}.title{font-size:38px;font-weight:bold}.sub{font-size:23px}.label{font-size:25px}.small{font-size:21px}.box{fill:white;stroke:#b8c5d5;stroke-width:2}</style>']
        self.text(55,65,title,'title');self.text(55,110,sub,'sub')
    def text(self,x,y,t,cls='label',anchor='start'):
        self.p.append(f'<text x="{x}" y="{y}" class="{cls}" text-anchor="{anchor}">{escape(t)}</text>')
    def box(self,x,y,w,h,title):
        self.p.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="18" class="box"/>')
        self.text(x+20,y+42,title)
    def wire(self,x1,y1,x2,y2,color,label=None):
        self.p.append(f'<path d="M{x1},{y1} L{x2},{y2}" stroke="{C.get(color,color)}" stroke-width="5" fill="none"/>')
        for x,y in [(x1,y1),(x2,y2)]:self.p.append(f'<circle cx="{x}" cy="{y}" r="6" fill="{C.get(color,color)}"/>')
        if label:self.text((x1+x2)/2,y1-14,label,'small','middle')
    def save(self,name):
        self.text(55,1060,'MiniDash · 2026-11-05 · Контакты показаны по именам, не по расположению на плате.','small')
        self.p.append('</svg>');(OUT/f'{name}.svg').write_text('\n'.join(self.p))
        try:
            import cairosvg
            cairosvg.svg2png(bytestring='\n'.join(self.p).encode(),write_to=str(OUT/f'{name}.png'))
        except ImportError:pass

def can(device, tx, rx, name):
    s=Sheet(f'{device} → SN65HVD230 → OBD-II',f'CAN 500 кбит/с · TX GPIO{tx}, RX GPIO{rx} · сверить с выбранным sdkconfig')
    s.box(60,180,420,540,device);s.box(820,180,420,540,'Модуль SN65HVD230')
    rows=[('3V3','VCC / 3V3','v33'),('GND','GND','gnd'),(f'GPIO{tx} / TX','CTX / TXD / D','tx'),(f'GPIO{rx} / RX','CRX / RXD / R','rx')]
    for i,(a,b,col) in enumerate(rows):
        y=290+i*110;s.text(90,y-17,a);s.text(850,y-17,b);s.wire(440,y,840,y,col,'3.3 В' if i==0 else '← приём' if i==3 else '→' if i==2 else 'общая масса')
    s.text(1270,285,'OBD 6');s.text(1270,395,'OBD 14');s.text(1080,270,'CANH');s.text(1080,380,'CANL')
    s.wire(1200,300,1500,300,'h');s.wire(1200,410,1500,410,'l');s.text(1270,465,'витая пара','small')
    s.text(85,775,'Питание платы: только 5 В на вход 5V/VIN или USB; схема питания - отдельный лист.','small')
    s.text(85,825,'GND платы, трансивера и DC/DC общие. OBD4 - возврат питания; OBD5 - опорная масса.','small')
    s.text(85,875,'Для автомобильного ответвления дополнительный 120 Ω не устанавливать. Проверить модуль.','small')
    s.text(85,925,'S3: исторический R2 снят. CYD: наличие и монтаж резистора ещё проверить.','small')
    s.text(85,975,'На CN1 CYD искать подписи 3V3/GND/IO22/IO27; порядок контактов не угадывать.','small')
    s.save(name)
    return [(device,a,'SN65HVD230',b,col) for a,b,col in rows]+[('SN65HVD230','CANH','OBD','6','h'),('SN65HVD230','CANL','OBD','14','l')]

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    nets=can('ESP32-S3',15,5,'02-s3-can')+can('CYD ESP32',22,27,'04-cyd-can')
    s=Sheet('Питание и общая масса · S3 / CYD','Прототип с LM2596S: схема соединений, не квалифицированный автомобильный источник')
    s.box(55,180,235,210,'OBD-II');s.text(80,295,'16: +12 В');s.text(80,350,'4: GND')
    s.box(370,180,310,210,'F1 + KCD11');s.text(395,285,'F1 → выключатель','small');s.text(395,340,'0.5–1 A: проверить ток','small')
    s.box(760,180,320,210,'LM2596S → 5 В');s.text(785,285,'IN+        OUT+','small');s.text(785,340,'IN−        OUT−','small')
    s.box(1200,180,345,210,'S3 / CYD');s.text(1230,285,'5V / VIN');s.text(1230,340,'GND')
    s.wire(260,300,390,300,'v5');s.wire(650,300,785,300,'v5');s.wire(1050,300,1230,300,'v5','5.0 В')
    s.wire(260,360,260,480,'gnd');s.wire(260,480,1430,480,'gnd')
    for x,y in [(805,360),(1040,360),(1430,360)]:s.wire(x,y,x,480,'gnd')
    s.text(610,525,'ОБЩАЯ GND: IN− и OUT− у этого DC/DC не изолированы','small')
    s.text(80,585,'OBD5 подключается как опорная масса по проверенному жгуту; не использовать вместо OBD4.','small')
    s.text(80,630,'В известном удлинителе 4 и 5 звонятся на один провод: перепроверить именно свой кабель.','small')
    s.box(60,675,1470,320,'Перед первым включением')
    for i,t in enumerate([
        '1. Отсоединить платы; измерить OUT+ относительно OUT−. На плату подавать только стабильные 5 В.',
        '2. 12 В не подключать к ESP, CYD или CAN-модулю. SN65HVD230 питается от 3V3 платы.',
        '3. USB ПК и внешний DC/DC одновременно не подключать без проверенной развязки источников.',
        '4. F1 ставить у источника. После пайки проверить полярность, отсутствие КЗ и механическую фиксацию.',
        '5. Постоянная установка требует защиты переполюсовки/импульсов и тепловой проверки DC/DC.'
    ]):s.text(85,760+i*44,t,'small')
    s.save('01-power')
    s=Sheet('ESP32-S3 → отдельный TFT 2.4″ + XPT2046','Профиль ST7789, 320×240 · GPIO из sdkconfig.defaults · существующий прототип')
    s.box(60,155,420,745,'ESP32-S3 2025-V1.4');s.box(990,155,550,745,'TFT SPI V1.3 / XPT2046')
    rows=[('3V3','VCC','v33'),('GND','GND','gnd'),('GPIO12','SCK + T_CLK','spi'),('GPIO11','SDI/MOSI + T_DIN','spi'),('GPIO10','CS (дисплей)','spi'),('GPIO8','DC / RS','spi'),('GPIO9','RESET / RST','spi'),('GPIO13','T_DO (не TFT SDO)','rx'),('GPIO14','T_CS','spi'),('GPIO4','LED: условно, см. ниже','tx')]
    for i,(a,b,col) in enumerate(rows):
        y=245+i*62;s.text(90,y-12,a,'small');s.text(1015,y-12,b,'small');s.wire(440,y,1010,y,col)
        nets.append(('ESP32-S3',a,'TFT/XPT2046',b,col))
    s.text(65,935,'T_IRQ и TFT SDO не подключены. Общий SPI для дисплея и тача; разные CS.','small')
    s.text(65,980,'GPIO4 → LED проверялось на этом модуле; ток входа не измерен. Для другого TFT нужен драйвер.','small')
    s.text(65,1018,'Нельзя одновременно соединять LED с GPIO4 и 3V3. Старые схемы TX17/RX16 здесь не применять.','small')
    s.save('03-s3-display')
    s=Sheet('Lite / Bluepill · только архитектурный эскиз','Не схема для пайки: не выбраны OLED, GPIO, носитель и завершённая STM32-прошивка')
    for x,y,w,h,t in [(70,200,380,220,'Источник 5 В'),(600,200,400,220,'STM32 / Bluepill?'),(1150,200,380,220,'OLED: не выбран'),(600,620,400,220,'CAN-трансивер 3.3 В'),(70,620,380,220,'OBD CANH / CANL'),(1150,620,380,220,'Кнопки: не назначены')]:s.box(x,y,w,h,t)
    s.wire(450,310,600,310,'v5');s.wire(1000,310,1150,310,'spi','SPI / I²C ?')
    s.wire(800,420,800,620,'tx');s.text(825,530,'CAN RX/TX: TBD','small');s.wire(450,730,600,730,'h');s.wire(1000,730,1150,730,'spi')
    s.text(80,930,'Есть переносимое C-ядро, но нет linked firmware, драйвера OLED и подтверждённого RAM/Flash fit.','small')
    s.text(80,980,'Корпуса Lite в комплекте - только механические концепты. Не переносить GPIO от S3 или CYD.','small')
    s.save('05-lite-concept')
    with (OUT/'nets.csv').open('w',newline='') as f:
        w=csv.writer(f);w.writerow(['from_device','from_terminal','to_device','to_terminal','color_key']);w.writerows(nets)

if __name__=='__main__':main()
