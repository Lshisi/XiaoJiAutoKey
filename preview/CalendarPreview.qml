import QtQuick
import "calendar" as Calendar

Calendar.Main {
    previewMode: true
    previewPayload: pluginApi.settings.demoData ? ({
        war: "大战！英雄南山马场",
        battle: "浮香丘",
        orecar: "跨服·渝州",
        school: "丐帮·美酒飘香",
        rescue: "少林·乱世",
        card: ["英雄无盐岛", "英雄天工坊", "英雄灵霄峡"],
        lucky: ["雀食", "鸿鸿", "圆圆"],
        weekly: {
            conn: ["洞天福地·守卫泥兰", "龙泉府·雪国冬猎"],
            raid: ["洛阳之战", "冰火岛·荒血路", "阆风悬城"]
        }
    }) : null
}
