"""对首次生成的 24 帧原图仅做裁切、脚底对齐及合图；需要 Pillow。"""

import json
from pathlib import Path

from PIL import Image

DESIGN = Path(__file__).resolve().parent
CELL = 256
FOOT_Y = 244
SOURCE_FOOT_Y = (
    (241, 241, 241, 241),
    (241, 241, 241, 241),
    (225, 217, 248, 248),
    (220, 220, 220, 220),
    (241, 241, 241, 240),
    (241, 240, 240, 240),
)
AIRBORNE = {(2, 0), (2, 1), (5, 2)}


def main():
    atlas = Image.new("RGBA", (1024, 1536))
    with Image.open(DESIGN / "cangjian-atlas-source.png") as source:
        for row in range(6):
            for column in range(4):
                left, top = column * CELL, row * CELL
                frame = source.crop((left, top, left + CELL, top + CELL)).convert("RGBA")
                foot = FOOT_Y - 16 if (row, column) in AIRBORNE else FOOT_Y
                aligned = Image.new("RGBA", (CELL, CELL))
                aligned.paste(frame, (0, foot - SOURCE_FOOT_Y[row][column]))
                atlas.paste(aligned, (left, top))
    atlas.save(DESIGN.parent / "skins" / "cangjian" / "atlas.png")

    skin = (DESIGN.parent / "ui" / "PetSkin.qml").read_text(encoding="utf-8")
    start = skin.index("{", skin.index("actions:"))
    actions = json.JSONDecoder().raw_decode(skin, start)[0]
    html = TEMPLATE.replace("__ACTIONS__", json.dumps(actions, ensure_ascii=False))
    (DESIGN / "sprite-preview.html").write_text(html, encoding="utf-8")

    preview = Image.new("RGBA", (1024, 512), "#f1ede3")
    positions = ((0, 0), (1, 2), (2, 0), (2, 2), (3, 0), (4, 2), (4, 0), (5, 2))
    for index, (row, column) in enumerate(positions):
        frame = atlas.crop((column * CELL, row * CELL, (column + 1) * CELL, (row + 1) * CELL))
        preview.alpha_composite(frame, (index % 4 * CELL, index // 4 * CELL))
    preview.convert("RGB").save(DESIGN / "cangjian-preview.png")
    print("已按首次原图生成 24 帧 PNG 图集及逐帧预览；未缩放或重绘角色和武器。")


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
#pet{width:256px;height:256px;background-image:url('../skins/cangjian/atlas.png');image-rendering:pixelated}
.controls{display:flex;gap:10px;flex-wrap:wrap;margin:12px 0}button,select{font:inherit;color:inherit;background:#32363b;border:1px solid #51555a;padding:9px 13px;border-radius:8px}
button{cursor:pointer}button:focus-visible,select:focus-visible{outline:2px solid #e6bf65;outline-offset:3px}
output{color:#e6bf65}.note{font-size:13px}option{background:#32363b}
</style>
<main>
<h1>藏剑小叽 · 精灵预览</h1>
<p>首次生成的 24 帧动作，保留角色、武器与姿态的原有比例。可暂停后逐帧查看。</p>
<div class="stage"><div id="pet" role="img" aria-label="藏剑小叽动作素材"></div></div>
<div class="controls">
<select id="action" aria-label="动作"><option value="idle">待机眨眼</option><option value="pet">摸摸头</option><option value="drag">拖起</option><option value="land">落地</option><option value="sleep">打盹</option><option value="practice">练剑</option><option value="rest">驻剑</option><option value="finish">收工</option></select>
<select id="size" aria-label="查看大小"><option value="160">小 · 160</option><option value="224">中 · 224</option><option value="256" selected>原格 · 256</option><option value="288">大 · 288</option></select>
<button id="toggle" type="button">暂停</button>
<button id="previous" type="button">上一帧</button><button id="next" type="button">下一帧</button>
</div>
<output id="position"></output>
<p class="note">当前仍为原型素材，帧间细节和动作连贯性待重制。此页面只展示素材，不连接宿主或操作按键。</p>
</main>
<script>
const actions=__ACTIONS__;
const pet=document.querySelector('#pet'),action=document.querySelector('#action'),size=document.querySelector('#size');
const toggle=document.querySelector('#toggle'),position=document.querySelector('#position');
let index=0,playing=true,timer=null;
function show(){const clip=actions[action.value],frame=clip.frames[index],side=Number(size.value);pet.style.width=pet.style.height=`${side}px`;pet.style.backgroundSize=`${side*4}px ${side*6}px`;pet.style.backgroundPosition=`${-frame.column*side}px ${-frame.row*side}px`;position.textContent=`第 ${index+1} / ${clip.frames.length} 帧 · ${frame.duration} ms`;toggle.textContent=playing?'暂停':'播放'}
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
