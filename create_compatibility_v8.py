"""Create a PowerPoint 2010-safe fallback by replacing Morph/Zoom with Fade/Wipe."""
from zipfile import ZipFile, ZIP_DEFLATED
from lxml import etree
import os, tempfile, shutil
SRC='output/چرا_به_قانون_نیاز_داریم_نسخه_۸_تیک‌تاک_کنفرانس.pptx'
OUT='output/چرا_به_قانون_نیاز_داریم_نسخه_۸_سازگار_۲۰۱۰.pptx'
P='http://schemas.openxmlformats.org/presentationml/2006/main'
with ZipFile(SRC) as zin, ZipFile(OUT+'.tmp','w',ZIP_DEFLATED) as zout:
    for info in zin.infolist():
        data=zin.read(info.filename)
        if info.filename.startswith('ppt/slides/slide') and info.filename.endswith('.xml'):
            root=etree.fromstring(data)
            tr=root.find('{%s}transition'%P)
            if tr is not None:
                for c in list(tr): tr.remove(c)
                # conservative, lightweight effect available in PowerPoint 2010
                tr.append(etree.Element('{%s}fade'%P))
                tr.set('spd','fast')
            data=etree.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
        zout.writestr(info,data)
os.replace(OUT+'.tmp',OUT)
print(OUT,round(os.path.getsize(OUT)/1048576,2),'MB')
