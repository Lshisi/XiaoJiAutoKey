"""同步现用精修主图集，并由同一份动作表生成多图集离线预览；需要 Pillow。"""

import json
from pathlib import Path

from PIL import Image

DESIGN = Path(__file__).resolve().parent
CELL = 256
SHEETS = {
    "base": {"file": "atlas.png", "columns": 4, "rows": 6},
    "practice": {"file": "practice-pixel.png", "columns": 6, "rows": 4},
    "ambient": {"file": "ambient-pixel.png", "columns": 4, "rows": 6},
}


def main():
    with Image.open(DESIGN / "cangjian-atlas-refined-source.png") as source:
        atlas = source.convert("RGBA")
    atlas.save(DESIGN.parent / "skins" / "cangjian" / "atlas.png")

    skin = (DESIGN.parent / "ui" / "PetSkin.qml").read_text(encoding="utf-8")
    start = skin.index("{", skin.index("actions:"))
    actions = json.JSONDecoder().raw_decode(skin, start)[0]
    html = TEMPLATE.replace("__ACTIONS__", json.dumps(actions, ensure_ascii=False))
    html = html.replace("__SHEETS__", json.dumps(SHEETS, ensure_ascii=False))
    (DESIGN / "sprite-preview.html").write_text(html, encoding="utf-8")

    preview = Image.new("RGBA", (1024, 512), "#f1ede3")
    positions = ((0, 0), (1, 2), (2, 0), (2, 2), (3, 0), (4, 2), (4, 0), (5, 2))
    for index, (row, column) in enumerate(positions):
        frame = atlas.crop((column * CELL, row * CELL, (column + 1) * CELL, (row + 1) * CELL))
        preview.alpha_composite(frame, (index % 4 * CELL, index // 4 * CELL))
    preview.convert("RGB").save(DESIGN / "cangjian-preview.png")
    print("已同步精修像素主图集与多图集逐帧查看；未重新绘制或缩放角色和武器。")


TEMPLATE = """
<!doctype html>
<html lang="zh-CN">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>藏剑小叽 · 精灵预览</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#1e2023;color:#eee9de;font:15px system-ui,sans-serif}
main{max-width:680px;margin:48px auto;padding:24px}h1{font-size:22px;font-weight:600}
p{color:#b9b7af;line-height:1.6}.stage{height:340px;display:grid;place-items:center;background:#292c30;border-radius:18px;margin:24px 0}
#pet{width:96px;height:96px;background-image:url('../skins/cangjian/atlas.png');image-rendering:auto}
.controls{display:flex;gap:10px;flex-wrap:wrap;margin:12px 0}button,select{font:inherit;color:inherit;background:#32363b;border:1px solid #51555a;padding:9px 13px;border-radius:8px}
button{cursor:pointer}button:focus-visible,select:focus-visible{outline:2px solid #e6bf65;outline-offset:3px}
output{color:#e6bf65}.note{font-size:13px}option{background:#32363b}
</style>
<main>
<h1>藏剑小叽 · 精灵预览</h1>
<p>保留藏剑像素小叽与轻描边，查看练剑、生活动作和睡醒过程。可暂停后逐帧查看。</p>
<div class="stage"><div id="pet" role="img" aria-label="藏剑小叽动作素材"></div></div>
<div class="controls">
<select id="action" aria-label="动作"><option value="idle">待机眨眼</option><option value="pet">摸摸头</option><option value="drag">拖起</option><option value="land">落地</option><option value="sleep">打盹</option><option value="practice">练剑</option><option value="rest">驻剑</option><option value="finish">收工</option><option value="look">张望</option><option value="stretch">伸懒腰</option><option value="yawn">打哈欠</option><option value="wake">醒来</option></select>
<select id="size" aria-label="查看大小"><option value="72">小 · 72</option><option value="96" selected>中 · 96</option><option value="120">大 · 120</option><option value="256">原格 · 256</option></select>
<button id="toggle" type="button">暂停</button>
<button id="previous" type="button">上一帧</button><button id="next" type="button">下一帧</button>
</div>
<output id="position"></output>
<p class="note">当前仍为原型素材，帧间细节和动作连贯性待重制。此页面只展示素材，不连接宿主或操作按键。</p>
</main>
<script>
const actions=__ACTIONS__;
const sheets=__SHEETS__;
const pet=document.querySelector('#pet'),action=document.querySelector('#action'),size=document.querySelector('#size');
const toggle=document.querySelector('#toggle'),position=document.querySelector('#position');
let index=0,playing=true,timer=null;
function show(){const clip=actions[action.value],frame=clip.frames[index],sheet=sheets[clip.sheet||'base'],side=Number(size.value);pet.style.width=pet.style.height=`${side}px`;pet.style.backgroundImage=`url('../skins/cangjian/${sheet.file}')`;pet.style.backgroundSize=`${side*sheet.columns}px ${side*sheet.rows}px`;pet.style.backgroundPosition=`${-frame.column*side}px ${-frame.row*side}px`;position.textContent=`第 ${index+1} / ${clip.frames.length} 帧 · ${frame.duration} ms`;toggle.textContent=playing?'暂停':'播放'}
function schedule(){clearTimeout(timer);const clip=actions[action.value];if(!playing||document.hidden||clip.frames.length===1)return;timer=setTimeout(()=>{if(index+1<clip.frames.length)index++;else if(clip.loopFrom>=0)index=clip.loopFrom;else{playing=false;show();return}show();schedule()},clip.frames[index].duration)}
action.addEventListener('change',()=>{index=0;playing=true;show();schedule()});
size.addEventListener('change',show);
toggle.addEventListener('click',()=>{const clip=actions[action.value];if(!playing&&clip.loopFrom<0&&index===clip.frames.length-1)index=0;playing=!playing;show();schedule()});
function step(delta){playing=false;const count=actions[action.value].frames.length;index=(index+delta+count)%count;show();schedule()}
document.querySelector('#previous').addEventListener('click',()=>step(-1));
document.querySelector('#next').addEventListener('click',()=>step(1));
document.addEventListener('visibilitychange',schedule);show();schedule();
</script>
</html>
"""


if __name__ == "__main__":
    main()
