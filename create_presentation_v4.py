from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.enum.shapes import MSO_SHAPE
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.dml.color import RGBColor
from pptx.oxml.xmlchemy import OxmlElement
from pptx.oxml.ns import qn
from lxml import etree
from PIL import Image, ImageDraw, ImageFilter, ImageEnhance
import os, zipfile

OUT='output/چرا_به_قانون_نیاز_داریم_نسخه_۴_شاهکار_۱۸_اسلاید.pptx'
A='assets_v3'; V='assets_v4'; os.makedirs(V,exist_ok=True); os.makedirs('output',exist_ok=True)
SW,SH=13.333,7.5
NAVY=(5,18,31); NAVY2=(10,35,57); INK=(12,31,48); PAPER=(237,242,241); WHITE=(248,252,255)
ICE=(201,237,255); GOLD=(255,198,78); MINT=(70,218,176); CORAL=(255,109,94); BLUE=(65,172,232); MUTED=(138,169,184)
FONT='Tahoma'
FA=str.maketrans('0123456789','۰۱۲۳۴۵۶۷۸۹')

# Clean visual sources generated in V3
sources={
 'justice':f'{A}/hero_justice.png','school':f'{A}/hero_school_clean.png','city':f'{A}/hero_city_clean.png',
 'traffic':f'{A}/traffic_3d.png','camp':f'{A}/camp_3d.png','constitution':f'{A}/constitution_3d.png'}

def cover_image(src,path,focus=.5,dark_side=None):
    im=Image.open(src).convert('RGB'); w,h=1920,1080; sc=max(w/im.width,h/im.height); im=im.resize((int(im.width*sc),int(im.height*sc)),Image.Resampling.LANCZOS)
    left=max(0,min(im.width-w,int((im.width-w)*focus))); top=(im.height-h)//2; im=im.crop((left,top,left+w,top+h))
    im=ImageEnhance.Contrast(im).enhance(1.06); im=ImageEnhance.Color(im).enhance(.92)
    if dark_side:
        ov=Image.new('RGBA',(w,h),(0,0,0,0)); d=ImageDraw.Draw(ov)
        for x in range(w):
            t=x/(w-1); a=int(210*((1-t)**1.55 if dark_side=='left' else t**1.55)); d.line((x,0,x,h),fill=(2,9,18,a))
        im=Image.alpha_composite(im.convert('RGBA'),ov).convert('RGB')
    im.save(path,'JPEG',quality=94,subsampling=0)

for name,src in sources.items():
    cover_image(src,f'{V}/{name}_L.jpg',.5,'left'); cover_image(src,f'{V}/{name}_R.jpg',.5,'right')

prs=Presentation(); prs.slide_width=Inches(SW); prs.slide_height=Inches(SH); blank=prs.slide_layouts[6]

def rgb(c): return RGBColor(*c)
def set_name(shape,name):
    try:
        if shape.shape_type==13: shape._element.nvPicPr.cNvPr.set('name',name)
        else: shape._element.nvSpPr.cNvPr.set('name',name)
    except: pass
    return shape

def rtl(p,align=PP_ALIGN.RIGHT):
    p.alignment=align; pr=p._p.get_or_add_pPr(); pr.set('rtl','1'); pr.set('algn','r' if align==PP_ALIGN.RIGHT else 'ctr')

def textbox(sl,x,y,w,h,content,size=24,color=INK,bold=False,align='right',val=MSO_ANCHOR.MIDDLE,margin=.08,name=None,line_spacing=1.05):
    sh=sl.shapes.add_textbox(Inches(x),Inches(y),Inches(w),Inches(h)); tf=sh.text_frame; tf.clear(); tf.word_wrap=True; tf.vertical_anchor=val
    tf.margin_left=tf.margin_right=Inches(margin); tf.margin_top=tf.margin_bottom=Inches(.03)
    p=tf.paragraphs[0]; p.text=content; rtl(p,PP_ALIGN.CENTER if align=='center' else PP_ALIGN.LEFT if align=='left' else PP_ALIGN.RIGHT); p.line_spacing=line_spacing
    for r in p.runs:
        r.font.name=FONT; r.font.size=Pt(size); r.font.bold=bold; r.font.color.rgb=rgb(color); r._r.get_or_add_rPr().set('lang','fa-IR')
    if name: set_name(sh,name)
    return sh

def rich(sl,x,y,w,h,runs,size=24,align='right',val=MSO_ANCHOR.MIDDLE,name=None):
    sh=sl.shapes.add_textbox(Inches(x),Inches(y),Inches(w),Inches(h)); tf=sh.text_frame; tf.clear(); tf.word_wrap=True; tf.vertical_anchor=val
    tf.margin_left=tf.margin_right=Inches(.06); tf.margin_top=tf.margin_bottom=Inches(.03); p=tf.paragraphs[0]; rtl(p,PP_ALIGN.CENTER if align=='center' else PP_ALIGN.RIGHT)
    for txt,col,bold in runs:
        r=p.add_run(); r.text=txt; r.font.name=FONT; r.font.size=Pt(size); r.font.bold=bold; r.font.color.rgb=rgb(col); r._r.get_or_add_rPr().set('lang','fa-IR')
    if name: set_name(sh,name)
    return sh

def shadow(sh,alpha=48,blur=62000,dist=26000):
    sp=sh._element.spPr; eff=sp.find(qn('a:effectLst'))
    if eff is None: eff=OxmlElement('a:effectLst'); sp.append(eff)
    o=OxmlElement('a:outerShdw'); o.set('blurRad',str(blur)); o.set('dist',str(dist)); o.set('dir','2700000'); o.set('algn','ctr'); o.set('rotWithShape','0')
    c=OxmlElement('a:srgbClr'); c.set('val','000000'); a=OxmlElement('a:alpha'); a.set('val',str(alpha*1000)); c.append(a); o.append(c); eff.append(o)

def shape(sl,kind,x,y,w,h,fill,line=None,trans=0,radius=True,name=None,shadowed=False):
    sh=sl.shapes.add_shape(kind,Inches(x),Inches(y),Inches(w),Inches(h)); sh.fill.solid(); sh.fill.fore_color.rgb=rgb(fill); sh.fill.transparency=trans
    if line: sh.line.color.rgb=rgb(line); sh.line.width=Pt(1.25)
    else: sh.line.fill.background()
    if shadowed: shadow(sh)
    if name: set_name(sh,name)
    return sh

def panel(sl,x,y,w,h,fill=NAVY2,line=BLUE,trans=3,name=None,accent=None):
    sh=shape(sl,MSO_SHAPE.ROUNDED_RECTANGLE,x,y,w,h,fill,line,trans,name=name,shadowed=True)
    if accent: shape(sl,MSO_SHAPE.ROUNDED_RECTANGLE,x+w-.055,y+.15,.045,h-.3,accent,None,0)
    return sh

def picture(sl,path,x,y,w,h,name=None):
    p=sl.shapes.add_picture(path,Inches(x),Inches(y),width=Inches(w),height=Inches(h)); shadow(p,52,72000,30000)
    if name: set_name(p,name)
    return p

def footer(sl,n,time='۰:۴۰',light=False):
    col=INK if light else ICE
    textbox(sl,.55,7.12,2.55,.18,'مطالعات اجتماعی • درس ۳',8.5,col,False,'left',margin=0)
    textbox(sl,10.65,7.08,.95,.2,f'زمان {time}',8.5,MUTED,False,'center',margin=0)
    textbox(sl,11.84,7.03,.82,.26,(str(n).zfill(2).translate(FA)+' / ۱۸'),10,GOLD,True,'right',margin=0)
    # lightweight chapter progress rail: native shapes, no GIF
    for i in range(18):
        c=GOLD if i==n-1 else ((55,84,101) if not light else (190,201,202))
        sh=shape(sl,MSO_SHAPE.RECTANGLE,.55+i*.285,6.88,.21,.035,c,None,0,name='!!Progress' if i==n-1 else None)

def header(sl,n,label,title,light=False):
    tc=INK if light else WHITE
    textbox(sl,.58,.28,2.8,.24,label,9.5,GOLD,True,'left',margin=0,name='!!SectionLabel')
    textbox(sl,.58,.64,12.05,.52,title,26,tc,True,'right',margin=0,name='!!MainTitle')

def dark_slide(n,label,title,time='۰:۴۰'):
    s=prs.slides.add_slide(blank); shape(s,MSO_SHAPE.RECTANGLE,0,0,SW,SH,NAVY,None); header(s,n,label,title); footer(s,n,time); return s

def light_slide(n,label,title,time='۰:۴۰'):
    s=prs.slides.add_slide(blank); shape(s,MSO_SHAPE.RECTANGLE,0,0,SW,SH,PAPER,None); header(s,n,label,title,True); footer(s,n,time,True); return s

def hero_slide(n,img,label,title,time='۰:۴۰',side='left'):
    s=prs.slides.add_slide(blank); picture(s,f'{V}/{img}_{"L" if side=="left" else "R"}.jpg',0,0,SW,SH,name='!!Scene')
    header(s,n,label,title); footer(s,n,time); return s

def tag(sl,x,y,w,txt,col=GOLD,light=False):
    shape(sl,MSO_SHAPE.ROUNDED_RECTANGLE,x,y,w,.38,(230,235,235) if light else (15,43,60),col,0,shadowed=False)
    textbox(sl,x+.08,y+.04,w-.16,.27,txt,10.5,col,True,'center',margin=0)

def card(sl,x,y,w,h,head,body,col=GOLD,light=False,number=None):
    fill=(250,252,251) if light else (12,38,57); line=(205,214,213) if light else col
    panel(sl,x,y,w,h,fill,line,0,accent=col)
    textbox(sl,x+.24,y+.18,w-.48,.36,head,17,col,True,'right',margin=0)
    textbox(sl,x+.24,y+.68,w-.48,h-.86,body,13.8,INK if light else ICE,False,'right',MSO_ANCHOR.TOP,margin=0,line_spacing=1.12)
    if number: tag(sl,x+.22,y+h-.48,.55,number,col,light)

# 01 — full cinematic cover, minimal typography
s=prs.slides.add_slide(blank); picture(s,f'{V}/justice_L.jpg',0,0,SW,SH,name='!!Scene')
shape(s,MSO_SHAPE.RECTANGLE,.62,.56,.06,5.86,GOLD,None,name='!!GoldRail')
textbox(s,.92,.68,5.4,.28,'درس ۳ • مطالعات اجتماعی پایه هفتم',11,GOLD,True,'left',margin=0)
textbox(s,.9,1.38,5.65,1.45,'چرا به قانون\nنیاز داریم؟',37,WHITE,True,'right',margin=0,name='!!HeroTitle',line_spacing=.92)
rich(s,.92,3.15,5.45,.78,[('قانون، مسیرِ ',ICE,False),('نظم، امنیت و عدالت',GOLD,True),(' است.',ICE,False)],18)
shape(s,MSO_SHAPE.RECTANGLE,.92,4.22,4.95,.02,(99,151,178),None)
textbox(s,.92,4.5,5.2,.72,'گروه ۶ • کلاس ۷/۳\nامین کریم‌پور · عزیزی · طاها عابدی‌فر · گله‌دار · محمدرضا فولادی',12.5,WHITE,False,'right',margin=0,line_spacing=1.1)
tag(s,.92,5.72,2.7,'نسخهٔ ۴ • کنفرانس ۱۰ دقیقه‌ای',MINT); footer(s,1,'۰:۲۵')
# 02 — hook / morph continuation
s=hero_slide(2,'justice','پردهٔ اول • مسئله','اگر فقط یک روز، هیچ قانونی وجود نداشت…','۰:۴۰','left')
textbox(s,.86,1.56,5.35,1.65,'چه کسی از حقِ\nضعیف‌ترها دفاع می‌کرد؟',28,WHITE,True,'right',margin=0,name='!!HeroTitle',line_spacing=.95)
shape(s,MSO_SHAPE.RECTANGLE,.87,3.62,4.65,.025,GOLD,None)
textbox(s,.88,3.92,4.95,1.25,'خیابان، مدرسه و حتی خانه خیلی زود دچار آشفتگی می‌شدند. قانون فقط «نباید» نیست؛ یک توافق جمعی برای زندگی امن است.',16,ICE,False,'right',MSO_ANCHOR.TOP,margin=0,line_spacing=1.18)
tag(s,.88,5.5,3.9,'سؤال آغازین: آزادی بدون مرز یا بی‌نظمی؟',CORAL)
# 03 — editorial route
s=light_slide(3,'نقشهٔ روایت','سه پرده؛ از یک پرسش تا یک پاسخ','۰:۳۵')
acts=[('پردهٔ اول','مسئله','حق من کجا تمام می‌شود؟',CORAL),('پردهٔ دوم','کشف','قانون چه چیزی را حفظ می‌کند؟',BLUE),('پردهٔ سوم','انتخاب','من چه مسئولیتی دارم؟',MINT)]
for i,(a,h,b,c) in enumerate(acts):
    x=.72+i*4.15; shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,x,1.68,3.76,3.72,WHITE,(205,215,216),0,shadowed=True,name='!!ActCard' if i==0 else None)
    textbox(s,x+.28,1.98,3.2,.25,a,10,c,True,'center',margin=0)
    textbox(s,x+.28,2.55,3.2,.55,h,26,INK,True,'center',margin=0)
    shape(s,MSO_SHAPE.OVAL,x+1.48,3.38,.78,.78,c,None)
    textbox(s,x+1.48,3.5,.78,.35,str(i+1).translate(FA),17,WHITE,True,'center',margin=0)
    textbox(s,x+.35,4.47,3.08,.48,b,13,INK,False,'center',margin=0)
textbox(s,2.5,5.95,8.3,.35,'هدف ارائه: دیدنِ قانون در تصمیم‌های کوچکِ هر روز',15,GOLD,True,'center',margin=0)
# 04 — social network, no cards repetition
s=dark_slide(4,'پایهٔ مفهوم','اجتماع، یک شبکهٔ زنده از حق‌ها و مسئولیت‌هاست','۰:۴۵')
# central node
shape(s,MSO_SHAPE.OVAL,5.28,2.28,2.75,2.75,(18,71,88),MINT,0,name='!!Core',shadowed=True)
textbox(s,5.62,2.92,2.05,.75,'زندگی\nاجتماعی',24,WHITE,True,'center',margin=0)
nodes=[(1.05,1.62,'خانواده','آرامش ↔ احترام',GOLD),(9.92,1.62,'مدرسه','آموزش ↔ نظم',BLUE),(1.05,4.75,'محله','امکانات ↔ مراقبت',MINT),(9.92,4.75,'جامعه','امنیت ↔ قانون‌مداری',CORAL)]
for x,y,h,b,c in nodes:
    shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,x,y,2.35,1.0,(13,42,60),c,0,shadowed=True)
    textbox(s,x+.15,y+.12,2.05,.28,h,14,c,True,'center',margin=0); textbox(s,x+.15,y+.52,2.05,.24,b,10.5,ICE,False,'center',margin=0)
# connectors behind-ish
for x1,y1,x2,y2,c in [(3.4,2.12,5.28,2.92,GOLD),(8.03,2.92,9.92,2.12,BLUE),(3.4,5.25,5.28,4.0,MINT),(8.03,4.0,9.92,5.25,CORAL)]:
    ln=s.shapes.add_connector(1,Inches(x1),Inches(y1),Inches(x2),Inches(y2)); ln.line.color.rgb=rgb(c); ln.line.width=Pt(2)
textbox(s,3.5,5.95,6.3,.38,'رابطه بیشتر ← اثر بیشتر ← مسئولیت بیشتر',15,GOLD,True,'center',margin=0)
# 05 — right boundary formula
s=light_slide(5,'اصل طلایی','آزادیِ واقعی، با مسئولیت کامل می‌شود','۰:۴۵')
textbox(s,.75,1.48,7.6,1.05,'«مرزِ حقِ من، جایی است که حقِ دیگری آغاز می‌شود.»',25,INK,True,'right',margin=0)
shape(s,MSO_SHAPE.RECTANGLE,.76,2.73,6.85,.035,GOLD,None)
formula=[('حق من',MINT),('+',MUTED),('قانون',GOLD),('+',MUTED),('حق دیگران',CORAL),('=',MUTED),('آزادی مسئولانه',BLUE)]
xx=.78
for txt,c in formula:
    w=1.65 if len(txt)>6 else .55 if txt in '+=' else 1.2
    if txt in '+=': textbox(s,xx,3.22,w,.55,txt,24,c,True,'center',margin=0)
    else:
        shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,xx,3.08,w,.82,WHITE,c,0,shadowed=True); textbox(s,xx+.08,3.28,w-.16,.34,txt,13,c,True,'center',margin=0)
    xx+=w+.18
# decision test
shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,9.1,1.55,3.45,4.55,NAVY,(28,71,92),0,shadowed=True)
textbox(s,9.45,1.95,2.75,.35,'آزمون ۳ ثانیه‌ای',18,GOLD,True,'center',margin=0)
for i,q in enumerate(['آسیب می‌زند؟','اگر همه انجام دهند؟','پیامدش را می‌پذیرم؟']):
    y=2.78+i*.9; tag(s,9.5,y,.52,str(i+1).translate(FA),[CORAL,GOLD,MINT][i]); textbox(s,10.18,y+.02,1.8,.3,q,12.5,ICE,True,'right',margin=0)
textbox(s,1.0,5.62,7.2,.4,'مسئولیت، دشمن آزادی نیست؛ شرطِ ماندگارشدن آن است.',16,GOLD,True,'center',margin=0)
# 06 — visual traffic
s=hero_slide(6,'traffic','پردهٔ دوم • کشف','هفت ثانیه عجله؛ چندین حق از دست‌رفته','۰:۴۵','right')
shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,7.25,1.48,5.35,4.7,(3,16,29),GOLD,10,shadowed=True)
textbox(s,7.65,1.86,4.55,.72,'چراغ قرمز فقط یک رنگ نیست؛\nقراردادِ حفظ جان است.',21,WHITE,True,'right',margin=0)
for i,(n,t,c) in enumerate([('جان','خطر تصادف',CORAL),('زمان','ترافیک و تأخیر',GOLD),('آرامش','تنش و بی‌اعتمادی',MINT)]):
    y=3.02+i*.82; textbox(s,7.7,y,1.1,.3,n,14,c,True,'right',margin=0); shape(s,MSO_SHAPE.RECTANGLE,8.95,y+.15,.62,.025,c,None); textbox(s,9.72,y,2.25,.3,t,12.5,ICE,False,'right',margin=0)
tag(s,8.0,5.65,3.85,'قانون رانندگی = زمانِ بیشتر برای زندگی',BLUE)
# 07 — chain reaction / morph detail
s=dark_slide(7,'زنجیرهٔ پیامد','یک تخلف کوچک، چگونه به بحران تبدیل می‌شود؟','۰:۴۰')
chain=[('۱','عبور ممنوع',CORAL),('۲','تداخل مسیر',GOLD),('۳','قفل تقاطع',BLUE),('۴','ترافیک و تنش',MINT),('۵','تصادف و ناامنی',CORAL)]
for i,(n,t,c) in enumerate(chain):
    x=.48+i*2.55; y=2.38+(i%2)*.35
    shape(s,MSO_SHAPE.HEXAGON,x,y,2.15,1.55,(12,42,60),c,0,shadowed=True,name='!!Chain' if i==0 else None)
    textbox(s,x+.25,y+.24,1.65,.3,n,12,c,True,'center',margin=0); textbox(s,x+.22,y+.75,1.72,.42,t,12.5,WHITE,True,'center',margin=0)
    if i<4: textbox(s,x+2.12,y+.58,.46,.34,'←',18,GOLD,True,'center',margin=0)
textbox(s,1.38,4.85,10.55,.62,'قانون، جلوی «سرایتِ خطا» را می‌گیرد؛ چون جامعه یک شبکهٔ به‌هم‌پیوسته است.',17,ICE,True,'center',margin=0)
tag(s,4.72,5.9,3.9,'یک انتخابِ مسئولانه، ده‌ها حق را حفظ می‌کند.',GOLD)
# 08 — law vs regulation editorial comparison
s=light_slide(8,'تفکیک دقیق','مقررات و قانون؛ شبیه‌اند، اما یکسان نیستند','۰:۵۰')
# asymmetric split
shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,.72,1.48,5.35,4.62,WHITE,(202,213,214),0,shadowed=True)
textbox(s,1.02,1.8,4.7,.42,'مقررات',23,MINT,True,'right',margin=0)
tag(s,1.0,2.48,1.4,'محیط محدود',MINT,True); tag(s,2.55,2.48,1.35,'توافق گروه',MINT,True); tag(s,4.05,2.48,1.4,'اجرای محلی',MINT,True)
textbox(s,1.0,3.28,4.72,1.5,'خانه، کلاس، مدرسه یا اردو\nنمونه: ساعت خاموشی و نوبت نظافت',16,INK,False,'right',MSO_ANCHOR.TOP,margin=0,line_spacing=1.3)
shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,6.4,1.48,6.2,4.62,NAVY,(32,76,95),0,shadowed=True)
textbox(s,6.78,1.8,5.45,.42,'قانون',23,GOLD,True,'right',margin=0)
tag(s,6.78,2.48,1.55,'جامعه',GOLD); tag(s,8.5,2.48,1.8,'مرجع رسمی',GOLD); tag(s,10.48,2.48,1.72,'ضمانت اجرا',GOLD)
textbox(s,6.78,3.28,5.4,1.5,'برای همهٔ افرادِ مشمول لازم‌الاجراست\nنمونه: رانندگی، مالکیت و اموال عمومی',16,ICE,False,'right',MSO_ANCHOR.TOP,margin=0,line_spacing=1.3)
textbox(s,2.2,6.33,8.95,.32,'وجه مشترک: هر دو برای نظم و احترام به حق دیگران طراحی می‌شوند.',14,GOLD,True,'center',margin=0)
# 09 — four reasons visual radial
s=dark_slide(9,'هستهٔ درس','قانون، چهار مسئلهٔ اصلی جامعه را حل می‌کند','۰:۵۵')
shape(s,MSO_SHAPE.OVAL,5.32,2.25,2.68,2.68,(32,58,65),GOLD,0,shadowed=True,name='!!Core')
textbox(s,5.7,3.0,1.92,.6,'چرا\nقانون؟',21,GOLD,True,'center',margin=0)
reasons=[(1.05,1.48,'حفظ حقوق','زور جای حق را نگیرد',GOLD),(9.93,1.48,'نظم و امنیت','رفتارها پیش‌بینی‌پذیر شوند',MINT),(1.05,4.72,'مسئولیت‌پذیری','پیامد رفتار را ببینیم',CORAL),(9.93,4.72,'حل اختلاف','دلیل جای دعوا را بگیرد',BLUE)]
for x,y,h,b,c in reasons:
    shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,x,y,2.38,1.22,(11,39,58),c,0,shadowed=True); textbox(s,x+.18,y+.17,2.02,.3,h,14.5,c,True,'center',margin=0); textbox(s,x+.18,y+.68,2.02,.28,b,10.3,ICE,False,'center',margin=0)
for x1,y1,x2,y2,c in [(3.43,2.09,5.32,2.92,GOLD),(8,2.92,9.93,2.09,MINT),(3.43,5.33,5.32,4.3,CORAL),(8,4.3,9.93,5.33,BLUE)]:
    ln=s.shapes.add_connector(1,Inches(x1),Inches(y1),Inches(x2),Inches(y2)); ln.line.color.rgb=rgb(c); ln.line.width=Pt(2)
# 10 — public property image
s=hero_slide(10,'city','قانون در شهر','مال عمومی، «مال هیچ‌کس» نیست؛ مالِ همه است','۰:۴۵','left')
shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,.72,1.46,5.65,4.75,(3,17,31),MINT,9,shadowed=True)
textbox(s,1.06,1.9,4.9,.5,'سه صحنه؛ یک مسئولیت',21,MINT,True,'right',margin=0)
rows=[('بوستان','پاکیزگی و حفظ گیاهان',MINT),('اتوبوس','مراقبت از تجهیزات و نوبت',GOLD),('کتابخانه','سکوت و امانت‌داری',BLUE)]
for i,(h,b,c) in enumerate(rows):
    y=2.85+i*.86; textbox(s,1.08,y,1.25,.3,h,14,c,True,'right',margin=0); shape(s,MSO_SHAPE.RECTANGLE,2.48,y+.13,.55,.025,c,None); textbox(s,3.18,y,2.55,.3,b,11.3,ICE,False,'right',margin=0)
textbox(s,1.06,5.56,4.9,.33,'هزینهٔ تخریب را دوباره همه می‌پردازند.',13,CORAL,True,'center',margin=0)
# 11 — camp visual challenge
s=hero_slide(11,'camp','کارگاه صفحهٔ ۱۵','اردو؛ جایی که یک قانون خوب فوراً دیده می‌شود','۰:۵۰','right')
shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,7.52,1.4,4.82,4.8,(4,18,31),GOLD,8,shadowed=True)
textbox(s,7.9,1.82,4.05,.46,'ماموریت گروه',21,GOLD,True,'right',margin=0)
textbox(s,7.9,2.52,4.03,1.05,'قاعده‌ای بنویسید که\nروشن، عادلانه و قابل اجرا باشد.',18,WHITE,True,'right',margin=0,line_spacing=1.15)
for i,(q,c) in enumerate([('چه رفتاری؟',MINT),('در چه زمان و مکان؟',BLUE),('اگر رعایت نشد، چه می‌شود؟',CORAL)]):
    y=3.92+i*.58; textbox(s,7.95,y,3.6,.3,q,12.2,c,True,'right',margin=0)
tag(s,8.35,5.72,3.3,'قانون خوب، مسئله را حل می‌کند.',GOLD)
# 12 — camp rules matrix, large typography
s=light_slide(12,'طراحی مقررات','پنج موقعیت اردو؛ پنج تصمیم کوتاه و دقیق','۰:۵۵')
items=[('چادر و وسایل','جای مشخص • تحویل سالم',GOLD),('استراحت','خاموشی توافقی • سکوت',BLUE),('آتش','محل مجاز • حضور مسئول',CORAL),('غذا','صف • سهم برابر • بدون اسراف',MINT),('کار گروهی','نقش روشن • گزارش نتیجه',GOLD)]
for i,(h,b,c) in enumerate(items):
    x=.68+(i%3)*4.17; y=1.45+(i//3)*2.22
    if i==4: x=4.85
    shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,x,y,3.78,1.76,WHITE,(201,211,212),0,shadowed=True)
    tag(s,x+.25,y+.22,1.35,h,c,True); textbox(s,x+.25,y+.88,3.25,.42,b,13.2,INK,True,'center',margin=0)
textbox(s,2.15,5.95,9.1,.36,'قاعدهٔ خوب = هدف روشن + رفتار مشخص + پیامد متناسب',16,GOLD,True,'center',margin=0)
# 13 — law vs ethics dramatic split
s=dark_slide(13,'دو قطب راهنما','قانون حداقل را تضمین می‌کند؛ اخلاق، بهترین را پیشنهاد می‌دهد','۰:۵۰')
# diagonal-ish blocks
shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,.65,1.45,5.75,4.72,(58,28,36),CORAL,0,shadowed=True,name='!!Ethics')
textbox(s,1.0,1.85,5.05,.46,'اخلاق',26,CORAL,True,'right',margin=0)
textbox(s,1.0,2.58,5.03,.85,'«انسانی‌ترین کار چیست؟»',19,WHITE,True,'right',margin=0)
textbox(s,1.0,3.62,5.0,1.2,'کمک به فرد نابینا\nگذشت و مهربانی\nرعایت حال سالمندان',15,ICE,False,'right',MSO_ANCHOR.TOP,margin=0,line_spacing=1.28)
shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,6.92,1.45,5.75,4.72,(46,42,24),GOLD,0,shadowed=True,name='!!Law')
textbox(s,7.3,1.85,5.0,.46,'قانون',26,GOLD,True,'right',margin=0)
textbox(s,7.3,2.58,5.0,.85,'«چه کاری باید انجام شود؟»',19,WHITE,True,'right',margin=0)
textbox(s,7.3,3.62,5.0,1.2,'ایست پشت چراغ قرمز\nرعایت مالکیت\nحفظ اموال عمومی',15,ICE,False,'right',MSO_ANCHOR.TOP,margin=0,line_spacing=1.28)
tag(s,4.65,5.92,4.0,'جامعهٔ مطلوب: قانون‌مدار + اخلاق‌مدار',MINT)
# 14 — constitution hierarchy with image window
s=hero_slide(14,'constitution','دانستنی تکمیلی','قانون اساسی؛ سقفی که همهٔ قوانین زیر آن قرار می‌گیرند','۰:۴۵','left')
shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,.68,1.46,5.75,4.78,(3,17,30),GOLD,8,shadowed=True)
levels=[('قانون اساسی','اصول بنیادین کشور',GOLD,4.35),('قوانین عادی','موضوع‌های عمومی جامعه',MINT,3.55),('مقررات اجرایی','جزئیات اجرا در نهادها',BLUE,2.75)]
for i,(h,b,c,w) in enumerate(levels):
    y=1.93+i*1.12; x=.98+(4.35-w)/2
    shape(s,MSO_SHAPE.TRAPEZOID,x,y,w,.86,(14,45,60),c,0,shadowed=True); textbox(s,x+.18,y+.12,w-.36,.25,h,13.5,c,True,'center',margin=0); textbox(s,x+.18,y+.46,w-.36,.2,b,9.5,ICE,False,'center',margin=0)
textbox(s,1.0,5.5,5.0,.36,'همه‌پرسی: ۱۲ آذر ۱۳۵۸',14,GOLD,True,'center',margin=0)
# 15 — rights/duties balance visual
s=hero_slide(15,'justice','تعادل شهروندی','حق و تکلیف؛ دو کفهٔ یک ترازو','۰:۵۰','right')
shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,7.55,1.42,4.82,4.9,(4,18,31),GOLD,8,shadowed=True)
textbox(s,7.92,1.82,4.05,.36,'حق',23,MINT,True,'right',margin=0)
textbox(s,7.92,2.38,4.05,.7,'آموزش • امنیت • احترام\nاستفاده از امکانات عمومی',14,ICE,False,'right',margin=0,line_spacing=1.2)
shape(s,MSO_SHAPE.RECTANGLE,7.92,3.32,3.95,.025,(75,112,129),None)
textbox(s,7.92,3.7,4.05,.36,'تکلیف',23,CORAL,True,'right',margin=0)
textbox(s,7.92,4.27,4.05,.72,'رعایت مقررات • حفظ اموال\nاحترام به نوبت و دیگران',14,ICE,False,'right',margin=0,line_spacing=1.2)
tag(s,8.12,5.55,3.55,'هیچ حقی بدون تکلیف پایدار نیست.',GOLD)
# 16 — decision framework, clean editorial
s=light_slide(16,'چارچوب تصمیم','پیش از عمل، سه سؤال از خودت بپرس','۰:۴۰')
questions=[('آسیب','چه کسی از این تصمیم آسیب می‌بیند؟',CORAL),('همگانی','اگر همه همین کار را کنند چه می‌شود؟',GOLD),('مسئولیت','آیا پیامد انتخابم را می‌پذیرم؟',MINT)]
for i,(h,b,c) in enumerate(questions):
    x=.72+i*4.15
    shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,x,1.62,3.75,3.62,WHITE,(202,213,214),0,shadowed=True,name='!!Decision' if i==0 else None)
    shape(s,MSO_SHAPE.OVAL,x+1.42,2.05,.9,.9,c,None,shadowed=True); textbox(s,x+1.42,2.25,.9,.35,str(i+1).translate(FA),18,WHITE,True,'center',margin=0)
    textbox(s,x+.3,3.2,3.15,.4,h,20,c,True,'center',margin=0)
    textbox(s,x+.4,3.92,2.95,.68,b,13,INK,False,'center',margin=0)
textbox(s,1.8,5.92,9.75,.42,'اگر پاسخ نگران‌کننده است، آزادی از مرزِ مسئولیت عبور کرده است.',16,GOLD,True,'center',margin=0)
# 17 — interactive challenge
s=dark_slide(17,'چالش ۶۰ ثانیه‌ای','قاضی کلاس باشید؛ حقِ نقض‌شده را پیدا کنید','۱:۰۰')
cases=[('صدای بلند نیمه‌شب','حقِ آرامش همسایه',CORAL),('عبور موتور از پیاده‌رو','حقِ امنیت عابر',GOLD),('نوشتن روی صندلی اتوبوس','حقِ استفادهٔ عمومی',MINT)]
for i,(q,a,c) in enumerate(cases):
    x=.68+i*4.18
    shape(s,MSO_SHAPE.ROUNDED_RECTANGLE,x,1.55,3.78,3.95,(11,39,58),c,0,shadowed=True)
    tag(s,x+.28,1.88,.62,str(i+1).translate(FA),c)
    textbox(s,x+.32,2.57,3.12,.74,q,17,WHITE,True,'center',margin=0)
    shape(s,MSO_SHAPE.RECTANGLE,x+.58,3.62,2.62,.025,(69,106,123),None)
    textbox(s,x+.4,4.12,2.98,.65,a,13.2,c,True,'center',margin=0)
tag(s,4.2,5.85,4.95,'پاسخ کامل = رفتار + حقِ نقض‌شده + پیامد',GOLD)
# 18 — finale, clean visual closure
s=prs.slides.add_slide(blank); picture(s,f'{V}/justice_L.jpg',0,0,SW,SH,name='!!Scene')
shape(s,MSO_SHAPE.RECTANGLE,.62,.62,.06,5.85,GOLD,None,name='!!GoldRail')
textbox(s,.92,.75,5.1,.28,'جمع‌بندی • یک جمله برای ماندن',11,GOLD,True,'left',margin=0)
textbox(s,.9,1.45,5.75,1.62,'قانون یعنی\nاحترام به حقِ خود و دیگران.',31,WHITE,True,'right',margin=0,name='!!HeroTitle',line_spacing=.95)
textbox(s,.92,3.48,5.38,.75,'حق را بشناس • تکلیف را انجام بده • پیامد را ببین',16,ICE,True,'right',margin=0)
shape(s,MSO_SHAPE.RECTANGLE,.92,4.5,4.95,.025,GOLD,None)
textbox(s,.92,4.85,5.25,.72,'سپاس از توجه شما\nگروه ۶ • کلاس ۷/۳',15,MINT,True,'right',margin=0,line_spacing=1.25)
tag(s,.92,5.78,3.85,'جامعهٔ بهتر، از انتخاب مسئولانهٔ من آغاز می‌شود.',GOLD); footer(s,18,'۰:۳۰')

# Lightweight, story-driven transitions. Morph is used only at purposeful visual continuities.
P159='http://schemas.microsoft.com/office/powerpoint/2015/09/main'
morph_slides={2,7,11,14,15,18}
for idx,sl in enumerate(prs.slides,1):
    root=sl._element; tr=OxmlElement('p:transition'); tr.set('spd','med'); tr.set('advClick','1')
    if idx in morph_slides:
        m=etree.Element('{%s}morph'%P159,nsmap={'p159':P159}); m.set('option','byObject'); tr.append(m)
    elif idx in {3,8,12,16}:
        ch=OxmlElement('p:wipe'); ch.set('dir','l'); tr.append(ch)
    elif idx in {4,9,13,17}:
        ch=OxmlElement('p:push'); ch.set('dir','l'); tr.append(ch)
    else:
        tr.append(OxmlElement('p:fade'))
    insert=1
    for j,e in enumerate(root):
        if e.tag.endswith('clrMapOvr'): insert=j+1
    root.insert(insert,tr)

prs.core_properties.title='چرا به قانون نیاز داریم؟ — نسخه ۴ شاهکار کنفرانسی'
prs.core_properties.subject='درس ۳ مطالعات اجتماعی پایه هفتم — ارائه ۱۸ اسلایدی'
prs.core_properties.author='گروه ۶ از کلاس ۷/۳'
prs.core_properties.comments='نسخه سبک و حرفه‌ای: بدون GIF و صدا؛ با تایپوگرافی درشت، تنوع چیدمان و Morph هدفمند'
prs.save(OUT)
print(OUT,round(os.path.getsize(OUT)/1024/1024,2),'MB')
