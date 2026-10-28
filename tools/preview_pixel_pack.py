"""Render shared C pixel primitives and the exact generated 5x7 atlas.

This is a design preview, not an LCD capture or a replacement vehicle simulator.
Requires Pillow and the native minidash-pixel-tests executable (--preview).
"""
import argparse
import json
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw
from generate_pixel_font import GLYPHS, SCALES

CARDS=[(10,39,174,112),(190,39,120,78),(190,121,120,86),(10,157,174,57)]
def text(draw,x,y,value,size,ink):
    scale=SCALES[size]; y+=size-7*scale
    for char in value:
        for row,bits in enumerate(GLYPHS.get(char,GLYPHS['?'])):
            for col in range(5):
                if bits&(1<<(4-col)):
                    left=x+col*scale;top=y+row*scale
                    draw.rectangle((left,top,left+scale-1,top+scale-1),fill=ink)
        x+=6*scale if size!=12 else 7

def paint_rects(draw,rectangles):
    for x,y,w,h,ink in rectangles:draw.rectangle((x,y,x+w-1,y+h-1),fill='#%06x'%ink)

def base(theme):
    p=['#%06x'%n for n in theme['palette']]
    image=Image.new('RGB',(320,240),p[0]);draw=ImageDraw.Draw(image)
    paint_rects(draw,theme['decor'])
    text(draw,10,8,'DRIVE',16,p[4]);text(draw,280,8,'DEMO',12,p[6])
    text(draw,17,44,'RPM',10,p[5]);text(draw,143,44,'DEMO',10,p[6])
    text(draw,17,66,'3850',48,p[4]);text(draw,154,133,'rpm',12,p[5])
    text(draw,197,44,'SPEED',10,p[5]);text(draw,197,63,'96',28,p[4]);text(draw,273,94,'km/h',10,p[5])
    text(draw,197,126,'COOLANT',10,p[5]);text(draw,197,145,'91',28,p[4]);text(draw,295,183,'C',12,p[5])
    text(draw,16,160,'BOOST',10,p[5]);text(draw,16,176,'0.85 bar',16,p[4])
    text(draw,10,219,'< PREV    HOLD: SETTINGS    NEXT >',10,p[5])
    return image

HTML='''<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>MiniDash Pixel Pack</title><style>
*{box-sizing:border-box}body{margin:0;background:#080c16;color:#fff2c9;font:16px system-ui,sans-serif;min-height:100vh;padding:clamp(20px,5vw,72px)}
main{max-width:1180px;margin:auto}.eyebrow{color:#46e5d2;letter-spacing:.22em;font-size:12px}h1{font-size:clamp(36px,6vw,76px);line-height:1;margin:22px 0}p{color:#a2b2d7;line-height:1.6;max-width:560px}.layout{display:grid;grid-template-columns:1fr 1.3fr;gap:40px;align-items:center;margin-top:42px}.frame{background:#111825;padding:18px;border:2px solid #33415c;box-shadow:8px 8px #03050a}canvas{display:block;width:100%;image-rendering:pixelated;aspect-ratio:4/3;background:black}button{background:#121a2b;color:#c8d4e8;border:1px solid #33415c;padding:13px 17px;cursor:pointer;font:inherit}button[aria-pressed=true],button:hover{border-color:#46e5d2;color:#46e5d2}.modes{display:flex;flex-wrap:wrap;gap:9px;margin:28px 0}.sub{font-size:12px;color:#8293b1}.tag{padding:8px 0;color:#46e5d2;font-size:12px;letter-spacing:.1em}@media(max-width:800px){.layout{grid-template-columns:1fr}h1{font-size:52px}}@media(prefers-reduced-motion:reduce){.motion{display:none}}
</style><main><div class="eyebrow">VAG MINIDASH / CYD</div><div class="layout"><section><h1>PIXEL<br>PACK.</h1><p>Большие цифры. Пиксельные панели. Три настроения старой игровой графики — для привычных диагностических экранов.</p><div class="modes" id="modes"></div><div class="tag" id="effect"></div><button class="motion" id="replay">Повторить переход ↻</button><p class="sub">Дизайн-предпросмотр 320×240 с общими C-примитивами и тем же атласом шрифта. Значения демонстрационные. Это не снимок LCD и не показания автомобиля.</p></section><section class="frame"><canvas width="320" height="240" id="screen" aria-label="Пиксельная диагностическая панель"></canvas></section></div></main><script>
const data=__DATA__,atlas=__ATLAS__,canvas=document.getElementById('screen'),ctx=canvas.getContext('2d');let selected=0,started=0,raf=0;
const names=['ARCADE / 16','DUNGEON / 90s','HANDHELD / 8'],effects=['Блочные ряды · 320 мс','Падающие колонны · 420 мс','Мозаичное раскрытие · 320 мс'];
function rects(rows){for(const[x,y,w,h,c]of rows){ctx.fillStyle='#'+c.toString(16).padStart(6,'0');ctx.fillRect(x,y,w,h)}}
function text(x,y,str,size,ink){const scale=({10:1,12:1,16:2,28:3,48:5})[size];y+=size-7*scale;ctx.fillStyle=ink;for(const c of str){const rows=atlas[c]||atlas['?'];for(let r=0;r<7;r++)for(let col=0;col<5;col++)if(rows[r]&(1<<(4-col)))ctx.fillRect(x+col*scale,y+r*scale,scale,scale);x+=size===12?7:6*scale}}
function base(){const t=data.themes[selected],p=t.palette.map(n=>'#'+n.toString(16).padStart(6,'0'));ctx.fillStyle=p[0];ctx.fillRect(0,0,320,240);rects(t.decor);text(10,8,'DRIVE',16,p[4]);text(280,8,'DEMO',12,p[6]);text(17,44,'RPM',10,p[5]);text(143,44,'DEMO',10,p[6]);text(17,66,'3850',48,p[4]);text(154,133,'rpm',12,p[5]);text(197,44,'SPEED',10,p[5]);text(197,63,'96',28,p[4]);text(273,94,'km/h',10,p[5]);text(197,126,'COOLANT',10,p[5]);text(197,145,'91',28,p[4]);text(295,183,'C',12,p[5]);text(16,160,'BOOST',10,p[5]);text(16,176,'0.85 bar',16,p[4]);text(10,219,'< PREV    HOLD: SETTINGS    NEXT >',10,p[5])}
function step(now){base();const duration=selected===1?420:320,progress=Math.min(1,(now-started)/duration);rects(data.themes[selected].frames[Math.min(20,Math.floor(progress*20))]);if(progress<1)raf=requestAnimationFrame(step)}
function replay(){cancelAnimationFrame(raf);if(matchMedia('(prefers-reduced-motion: reduce)').matches){base();return}started=performance.now();raf=requestAnimationFrame(step)}
const buttons=names.map((name,i)=>{const b=document.createElement('button');b.textContent=name;b.onclick=()=>{selected=i;buttons.forEach((x,j)=>x.setAttribute('aria-pressed',j===i));document.getElementById('effect').textContent=effects[i];replay()};document.getElementById('modes').append(b);return b});document.getElementById('replay').onclick=replay;buttons[0].click();
</script></html>'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--harness',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    data=json.loads(subprocess.check_output([str(args.harness.resolve()),'--preview'],text=True))
    stills=[]
    for theme in data['themes']:
        image=base(theme);stills.append(image)
        stem=theme['name'].lower().replace(' ','-')
        image.resize((960,720),Image.Resampling.NEAREST).save(args.output/(stem+'.png'))
        frames=[]
        for rectangles in theme['frames']:
            frame=image.copy();paint_rects(ImageDraw.Draw(frame),rectangles)
            frames.append(frame.resize((640,480),Image.Resampling.NEAREST))
        # GIF delays use 10-ms units; distribute ticks to keep total duration.
        ticks=42 if theme['id']==12 else 32
        minimum,extra=divmod(ticks,20)
        durations=[(minimum+(i<extra))*10 for i in range(20)]+[1800]
        frames[0].save(args.output/(stem+'.gif'),save_all=True,append_images=frames[1:],duration=durations,loop=0)
    board=Image.new('RGB',(1008,300),'#080c16');draw=ImageDraw.Draw(board)
    for i,(theme,image) in enumerate(zip(data['themes'],stills)):
        board.paste(image,(8+i*336,44));draw.text((8+i*336,14),theme['name'],fill='#dce5f5')
    board.save(args.output/'pixel-pack-overview.png')
    html=HTML.replace('__DATA__',json.dumps(data,separators=(',',':'))).replace('__ATLAS__',json.dumps(GLYPHS,separators=(',',':')))
    (args.output/'pixel-pack-preview.html').write_text(html,encoding='utf-8')
    print('Pixel preview PNG/GIF/HTML created:',args.output)

if __name__=='__main__':main()
