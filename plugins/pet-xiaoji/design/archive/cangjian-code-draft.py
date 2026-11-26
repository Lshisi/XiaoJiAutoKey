"""藏剑分层像素母版小样：固定几何与色板，导出 SVG、PNG 和眨眼动图。"""

from pathlib import Path
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent
COLORS = {
    "ink": "#54351f", "gold": "#f3c54e", "goldLight": "#ffe393",
    "goldShade": "#d59832", "cream": "#fff2d7", "creamShade": "#e6c9a4",
    "olive": "#8b883d", "oliveLight": "#b4ad58", "oliveShade": "#65652f",
    "metal": "#c8c4ae", "metalShade": "#878778", "beak": "#e99a26",
    "eye": "#211d1b", "white": "#fffaf0", "gem": "#6a91a0",
}


def shape(tag, coordinates, color, border="ink"):
    return tag, coordinates, color, border


def layers(eyes="open"):
    result = [
        ("feet", [
            shape("polygon", [(23, 51), (29, 51), (29, 55), (27, 56), (24, 56), (22, 54)], "gold"),
            shape("polygon", [(36, 51), (42, 51), (43, 54), (41, 56), (38, 56), (36, 55)], "gold"),
        ]),
        ("back-sword", [
            shape("polygon", [(18, 55), (16, 58), (15, 55), (42, 20), (45, 22)], "metal"),
            shape("polygon", [(40, 20), (45, 14), (48, 16), (43, 23)], "oliveShade"),
            shape("polygon", [(41, 20), (45, 22), (47, 21), (42, 18), (40, 18)], "goldShade"),
            shape("polygon", [(46, 11), (49, 12), (50, 14), (48, 16), (45, 14)], "gold"),
            shape("rect", (46, 12, 47, 13), "goldLight", None),
        ]),
        ("wings", [
            shape("polygon", [(15, 30), (12, 31), (9, 35), (8, 40), (10, 45), (14, 48), (18, 42), (19, 34)], "goldShade"),
            shape("polygon", [(49, 30), (52, 31), (55, 35), (56, 40), (54, 45), (50, 48), (46, 42), (45, 34)], "goldShade"),
            shape("polygon", [(12, 35), (10, 38), (11, 42), (14, 44), (17, 38)], "gold", None),
            shape("polygon", [(52, 35), (54, 38), (53, 42), (50, 44), (47, 38)], "gold", None),
        ]),
        ("body", [
            shape("polygon", [(27, 13), (37, 13), (42, 15), (46, 19), (48, 24), (49, 29), (52, 36), (52, 44), (50, 49), (46, 53), (40, 55), (24, 55), (18, 53), (14, 49), (12, 44), (12, 36), (15, 29), (16, 24), (18, 19), (22, 15)], "gold"),
            shape("polygon", [(42, 17), (46, 21), (47, 28), (50, 36), (50, 44), (48, 48), (44, 51), (37, 53), (23, 53), (19, 51), (17, 48), (22, 50), (39, 50), (44, 46), (46, 40), (45, 30)], "goldShade", None),
            shape("polygon", [(23, 17), (28, 15), (36, 15), (40, 17), (35, 17), (29, 18), (23, 21), (20, 24), (20, 20)], "goldLight", None),
            shape("polygon", [(22, 32), (27, 34), (32, 32), (37, 34), (42, 32), (46, 35), (49, 41), (49, 46), (46, 50), (40, 53), (24, 53), (18, 50), (15, 46), (15, 41), (18, 35)], "cream", None),
            shape("polygon", [(47, 39), (49, 42), (49, 46), (46, 50), (40, 53), (24, 53), (18, 50), (17, 47), (23, 50), (39, 50), (44, 47), (47, 43)], "creamShade", None),
        ]),
        ("cape", [
            shape("polygon", [(17, 31), (20, 35), (15, 38), (10, 36), (12, 32)], "olive"),
            shape("polyline", [(12, 34), (15, 35), (18, 33)], "oliveLight", None),
            shape("polygon", [(46, 31), (50, 32), (54, 36), (51, 39), (47, 38), (43, 35)], "olive"),
            shape("polyline", [(47, 33), (49, 35), (52, 35)], "oliveLight", None),
        ]),
        ("tuft", [
            shape("polygon", [(28, 14), (28, 11), (30, 8), (34, 6), (38, 6), (34, 8), (32, 11), (32, 14)], "gold"),
            shape("polyline", [(30, 12), (31, 9), (34, 7)], "goldLight", None),
        ]),
        ("ornaments", [
            shape("polyline", [(17, 33), (21, 36), (26, 38), (36, 39), (43, 36), (47, 33)], "ink", None),
            shape("polyline", [(18, 33), (22, 35), (27, 37), (36, 38), (43, 35), (46, 33)], "goldLight", None),
            shape("polygon", [(25, 34), (26, 37), (29, 38), (26, 39), (25, 42), (24, 39), (21, 38), (24, 37)], "goldLight"),
            shape("rect", (24, 37, 25, 38), "gem", None),
            shape("polyline", [(25, 41), (25, 47)], "ink", None),
            shape("polygon", [(24, 46), (26, 46), (27, 50), (23, 50)], "oliveLight"),
            shape("polygon", [(34, 39), (31, 42), (33, 44), (36, 45), (39, 44), (41, 42), (38, 39), (36, 41)], "gold"),
            shape("polyline", [(36, 42), (33, 43)], "goldLight", None),
            shape("polyline", [(37, 42), (39, 43)], "goldLight", None),
            shape("polygon", [(28, 12), (25, 10), (23, 11), (23, 13), (26, 15), (28, 14)], "goldLight"),
            shape("rect", (28, 13, 29, 14), "goldShade"),
        ]),
        ("face", [shape("polygon", [(32, 28), (35, 30), (32, 33), (29, 30)], "beak")]),
    ]
    eye_shapes = []
    for center in (24, 40):
        if eyes == "closed":
            eye_shapes.append(shape("polyline", [(center-2, 26), (center-1, 27), (center+1, 27), (center+2, 26)], "eye", None))
        else:
            eye_shapes.append(shape("ellipse", (center-2, 23 if eyes == "open" else 25, center+2, 27), "eye", None))
            if eyes == "open":
                eye_shapes.append(shape("rect", (center-1, 23, center, 24), "white", None))
    result.append(("eyes", eye_shapes))
    return result


def render(eyes):
    picture = Image.new("RGBA", (64, 64))
    draw = ImageDraw.Draw(picture)
    svg = ET.Element("svg", xmlns="http://www.w3.org/2000/svg", width="256", height="256", viewBox="0 0 64 64", attrib={"shape-rendering": "crispEdges"})
    for name, shapes in layers(eyes):
        group = ET.SubElement(svg, "g", id=name)
        for tag, coordinates, color, border in shapes:
            fill = COLORS[color]
            outline = COLORS[border] if border else None
            attributes = {"fill": "none" if tag == "polyline" else fill}
            if tag in ("polygon", "polyline"):
                attributes["points"] = " ".join(f"{x},{y}" for x, y in coordinates)
                if tag == "polygon":
                    draw.polygon(coordinates, fill=fill, outline=outline)
                else:
                    draw.line(coordinates, fill=fill)
                    attributes["stroke"] = fill
            else:
                x1, y1, x2, y2 = coordinates
                if tag == "rect":
                    draw.rectangle(coordinates, fill=fill, outline=outline)
                    attributes.update(x=str(x1), y=str(y1), width=str(x2-x1+1), height=str(y2-y1+1))
                else:
                    draw.ellipse(coordinates, fill=fill, outline=outline)
                    attributes.update(cx=str((x1+x2)/2), cy=str((y1+y2)/2), rx=str((x2-x1+1)/2), ry=str((y2-y1+1)/2))
            if outline:
                attributes["stroke"] = outline
            ET.SubElement(group, tag, attributes)
    return picture.resize((256, 256), Image.Resampling.NEAREST), svg


def main():
    still, svg = render("open")
    ET.ElementTree(svg).write(ROOT / "cangjian-code-draft.svg", encoding="utf-8", xml_declaration=True)
    still.save(ROOT / "cangjian-code-draft.png")
    frames = []
    palette = Image.new("P", (1, 1))
    colors = ["#232221", *COLORS.values()]
    entries = [component for color in colors for component in tuple(bytes.fromhex(color[1:]))]
    palette.putpalette(entries + [0] * (768-len(entries)))
    for eye in ("open", "half", "closed", "half", "open"):
        frame, _ = render(eye)
        background = Image.new("RGBA", frame.size, colors[0])
        background.alpha_composite(frame)
        frames.append(background.convert("RGB").quantize(palette=palette, dither=Image.Dither.NONE))
    frames[0].save(ROOT / "cangjian-code-draft.gif", save_all=True, append_images=frames[1:], duration=[2200, 90, 100, 90, 1200], loop=0, disposal=2)
    print("已导出分层 SVG、透明 PNG 和固定色板眨眼 GIF 小样。")


if __name__ == "__main__":
    main()
