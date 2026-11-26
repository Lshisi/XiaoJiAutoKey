"""切帧、对齐、合图；按正式动作表导出无损 WebP 素材预览，需要 Pillow。"""

import json
from pathlib import Path

from PIL import Image


DESIGN = Path(__file__).resolve().parent
CELL = 256
SOURCE_FOOT_Y = (
    (244, 244, 244, 244),
    (244, 244, 244, 244),
    (221, 215, 244, 244),
    (218, 218, 218, 218),
    (244, 244, 244, 244),
    (244, 244, 210, 244),
)
AIRBORNE = {(2, 0), (2, 1), (5, 2)}


def main():
    atlas = Image.new("RGBA", (1024, 1536))
    frames = []
    with Image.open(DESIGN / "cangjian-atlas-source.png") as source:
        for row in range(6):
            for column in range(4):
                left, top = column * CELL, row * CELL
                original = source.crop((left, top, left + CELL, top + CELL))
                aligned = Image.new("RGBA", (CELL, CELL))
                foot_y = 208 if (row, column) in AIRBORNE else 224
                aligned.paste(original, (0, foot_y - SOURCE_FOOT_Y[row][column]))
                atlas.paste(aligned, (left, top))
                frames.append(aligned)

    destination = DESIGN.parent / "skins" / "cangjian" / "atlas.png"
    destination.parent.mkdir(parents=True, exist_ok=True)
    atlas.save(destination)

    skin_source = (DESIGN.parent / "ui" / "PetSkin.qml").read_text(encoding="utf-8")
    actions_start = skin_source.index("{", skin_source.index("actions:"))
    actions, _ = json.JSONDecoder().raw_decode(skin_source, actions_start)
    preview = []
    durations = []
    sequence = (("idle", 1), ("pet", 1), ("drag", 2), ("land", 1),
                ("sleep", 1), ("practice", 3), ("rest", 1), ("finish", 1))
    for action, cycles in sequence:
        clip = actions[action]
        for cycle in range(cycles):
            start = clip["loopFrom"] if cycle else 0
            for frame in clip["frames"][start:]:
                preview.append(frames[frame["row"] * 4 + frame["column"]])
                durations.append(frame["duration"])

    atlas.save(DESIGN / "cangjian-atlas-reference.webp", lossless=True, exact=True, method=6)
    preview[0].save(
        DESIGN / "cangjian-preview.webp",
        save_all=True,
        append_images=preview[1:],
        duration=durations,
        loop=0,
        lossless=True,
        exact=True,
        method=6,
    )
    print(destination)
    print(DESIGN / "cangjian-atlas-reference.webp")
    print(DESIGN / "cangjian-preview.webp")

    html = """<!doctype html>
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
<p>直接查看透明图集中的完整颜色。可选择动作，或暂停后逐帧检查。</p>
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
    (DESIGN / "sprite-preview.html").write_text(html.replace("__ACTIONS__", json.dumps(actions, ensure_ascii=False)), encoding="utf-8")
    print(DESIGN / "sprite-preview.html")


if __name__ == "__main__":
    main()
