#include "VersionInfo.h"

// 定义当前版本号
const QString VersionInfo::currentVersion = "3.0.0";

// 获取版本历史
QVector<VersionInfo::VersionEntry> VersionInfo::getVersionHistory() {
    QVector<VersionEntry> history;
    
    // 添加版本历史记录
    history.append({
        "v3.0.0",
        "2026-03-??",
        "• 首次发布\n• 完全重写编辑器，现代化C++ Qt6.0框架，原生级性能\n• 新增 多标签页 功能，多文件编辑更顺心\n• 优化 智能翻译 功能，支持自定义提示词和翻译配置\n• 新增 只读模式 功能，防止文件意外修改\n• 新增 批量替换 功能，可批量修改文本\n• 新增 批量导出 功能，选定相应标签页可批量导出\n• 新增 自动保存 功能，工作时自动保存文件，避免丢失工作内容\n• 新增 生成字符表 功能，可生成对应的dat\n• 新增 哈希转换 功能，打开圣安地列斯/GTA4的GXT时会显示键名而不是哈希值\n• 优化 保存 功能，可使用另存为功能\n• 新增 文本渲染 功能，将选中文本颜色标识符可视化，并使用对应字体\n• 新增 文件版本 支持，现支持GXT2、DAT编辑与WHM解析\n• 美化 图标 显示，使用FontAwesome图标替代Emoji，提供更美观界面\n• 新增 键名校验 功能，GTAIII和罪恶都市的键名编辑受限，超出限制则将被限制\n• 新增 过滤搜索 切换功能，可自由选择是否使用过滤式搜索\n• 新增 撤销重做 功能，防止编辑失误"
    });
    
    return history;
}
