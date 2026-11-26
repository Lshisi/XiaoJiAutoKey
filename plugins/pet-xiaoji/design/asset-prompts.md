# 藏剑小叽素材约定与生成提示

本轮以用户最终确认的方向为准：保留原藏剑像素小黄鸡，只减轻外轮廓的粗黑描边，并增加同一角色的动作。小黄鸡的服饰、橄榄绿披肩、银杏头饰、蓝色扣饰及背剑均延续首版；不采用手办风、3D 质感或另换一套国风角色设计。

[KuroCat](https://github.com/qianyiai/kurocat) 与用户提供的朋友作品仅作为动作反馈、呈现精细度的参考，没有引入它们的代码或角色素材。原始形象保留在 `cangjian-atlas-source.png`，已对齐的轻描边母版为 `cangjian-atlas-refined-source.png`。

内置图像接口两次返回响应格式错误后，实际生成使用当前配置的图片工具 `mcp__magpie_image__generate_image`，模型为 `https-sadai-top-v1/gpt-image-2.5-sunburst`。未执行 CLI 图像生成或项目制作脚本。下列图集已查看静态图片与透明通道；运行中的播放节奏、帧间一致性仍需用户确认。

## 输出与帧位

| 运行文件 | 图集尺寸 | 布局 | 用途 |
| --- | --- | --- | --- |
| `skins/cangjian/atlas.png` | 1024×1536 | 4 列、6 行 | 首版动作的轻描边版本 |
| `skins/cangjian/practice-pixel.png` | 1536×1024 | 6 列、4 行 | 24 帧连续练剑 |
| `skins/cangjian/ambient-pixel.png` | 1024×1536 | 4 列、6 行 | 张望、伸懒腰、打哈欠、醒来 |

生活图集使用与主图集相同的标准画幅，每格 256×256；张望与醒来各 8 格，其余两种动作各 4 格。图集使用真实 Alpha 透明通道。此前存在彩色杂边的正方形版本不进入运行资源。

## 新增练剑动作的提示

参考：首版已对齐的 `atlas.png`。生成配置：透明背景、`1536x1024`、质量 high。

```text
Create additional animation frames for the EXACT EXISTING PIXEL-ART GOLDEN CHICKEN in the reference image. This is a continuation of the SAME game sprite, NOT a redesign and NOT a style transfer. Copy the original limited warm yellow/cream/olive palette, stepped pixel-art outlines, tiny round black eyes, compact golden chick shape, white belly, small ginkgo crest, OLIVE green short cape with gold border, blue star clasp and tassel, and back sheathed sword. Absolutely NO 3D, plush, ceramic, shiny figure, painterly rendering, white martial-arts robes or new costume. Match the reference's specific pixel sprite rendering and proportions. Output1536x1024 transparent PNG, six columns by four rows, 24 square frame cells, chronological left-to-right row-major sword-practice cycle. Each character is centred local x128, feet y244 in its256square, head and back sword aligned the same as the reference row4. The front wing grips the same little practice sword from reference, with the sheathed sword still on the back. Frame progression: 1-4 low ready with slight weight shift; 5-8 draw the practice sword upward and behind shoulder; 9-12 bring it overhead then begin a strong diagonal downward slash; 13-16 sword sweeps distinctly across the front to the other side, blade changing angle each frame; 17-20 follow through and recover grip; 21-24 return smoothly to the EXACT initial low-ready stance. The training sword must visibly move through a large arc across the 24 frames, never stay in one held pose. Adjacent frames should be small changes in the same continuous movement. Keep costume, face, body size and pixel scale stable. Everything inside its own cell. Real alpha0 outside characters, no background or vignette, no baked grid/checkerboard, no floor shadows, no words or labels. Original pixel-art style is mandatory.
```

## 新增生活动作的提示

参考：轻描边版本的 `atlas.png`。生成配置：透明背景、`1024x1536`、质量 high。

```text
为参考图中的同一只藏剑像素小黄鸡制作生活动作补充图集，角色造型必须完全相同，保留轻暖棕描边、像素颗粒、金黄羽毛、白肚皮、橄榄绿金边披肩、蓝色小扣、银杏头饰、背剑，不重设计角色，不加汉服，不做手办或3D。轮廓沿用参考图的细暖棕色，不得加粗黑边。画幅严格1024x1536，与参考图相同，4列6行24个256x256格，按行从左到右阅读。每格同一角色、同一比例、同一镜头，站立脚底y244、身体x128，元素完全在格内。第0行和第1行共8帧为连续张望：从正面微微侧头向左看、停一下、转回、再向右看、回到正面，动作差异细小连续。第2行4帧伸懒腰：轻微缩身、抬起双翅、充分伸展闭眼、放下双翅站稳。第3行4帧打哈欠：半闭眼、张小嘴、抬翅遮嘴打哈欠、闭嘴恢复。第4行和第5行共8帧为醒来过程：保持原稿睡觉姿势、轻轻睁眼、抬头、支起身体、站起、伸展双翅、收翅、回到首版正面待机姿势。严格保持首版角色、服饰和像素画风，动作需要连贯，不得每帧变成不同形象。背景必须是真正干净的alpha0透明底，所有角色周围没有杂边。禁止纯红、荧光黄、荧光绿的边缘碎屑或抠图光圈；角色外围只能是细暖棕色轮廓，其外立刻全透明。不要任何背景、光晕、格线、文字、水印、底部投影。特别保证无脏边无彩色外轮廓，可以直接作为游戏透明像素动画图集。
```

## 主图集轻描边的最终提示

参考：首版已对齐的 `atlas.png`。精修完成后同步到主图集及设计母版。

```text
对参考图做非常克制的像素画描边精修。用户已经确认保留这只藏剑像素小黄鸡，唯一主要问题是轮廓黑边太粗、阶梯锯齿太硬。只改线条处理：把外轮廓近黑色粗边减轻为细一号的暖棕褐色线，大致#80572f，受光位置用更浅的金棕色衔接；原来3到4像素厚的黑边视觉上减轻到1到2像素，不要双重描边。柔化最突兀的阶梯拐角，但仍能辨认出原有细腻像素颗粒与像素画风，不变成光滑3D或厚涂。眼睛仍是原来的深黑色小眼睛，不要改眼睛、不加大高光。严格保留每一帧的角色、比例、姿态、表情、服饰、配饰、武器、位置、原有颜色与透明蒙版形状。金黄羽毛、白肚皮、橄榄绿披肩金边、蓝宝石扣、银杏头饰、背剑和练功剑都保持原图。不得重设计、不得加汉服、不得加毛绒或塑料质感。原图是1024x1536，4列6行24格，必须完整保留相同布局和每格边界，不能重新排列或漏帧，不能裁图。只让描边更轻、更自然，主体仍是原来的像素小叽。保留真正alpha0透明底，禁止背景、格线、字、水印、阴影底板。
```

## 练剑图集轻描边的最终提示

参考：本轮新增的原始 24 帧像素练剑图集。

```text
只精修参考像素动画图集的描边，不重绘或重设计角色，不改变任何动作。把过重近黑色外轮廓由粗硬黑边减轻成细一号的暖棕褐色线，参考#80572f，受光边缘用浅金棕衔接，视觉厚度1到2像素。最突兀的阶梯拐角略柔化，但保留像素画颗粒、原色与像素风，绝不变成3D、手办、毛绒、光滑插画或厚涂。眼睛仍为原有黑色小眼睛。必须完全保留这只藏剑小黄鸡的金黄羽毛、白肚皮、橄榄绿金边披肩、蓝扣、银杏头饰、背剑和前翼练功剑。严格保留1536x1024、6列4行、24帧连续挥剑动画的格子布局、每帧身体与武器位置、动作顺序及画幅；不要换姿势或重新排布。只调整轮廓像素的颜色和视觉粗细。输出相同尺寸透明PNG，原角色外alpha0透明，禁止任何背景、渐变、格线、文字、水印。
```
