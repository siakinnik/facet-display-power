// Russian translation of the display-power plugin. Keys are the English source strings.
#include "i18n/i18n.h"

namespace dp {

namespace {

const facet::i18n::Table& ru() {
    static const facet::i18n::Table table = {
        // Tile and status
        {"Screen on", "Экран вкл"},
        {"Screen off", "Экран выкл"},
        {"Screen & camera", "Экран и камера"},
        {"Now", "Сейчас"},
        {"Screen", "Экран"},
        {"on — {}", "включён — {}"},
        {"off — {}", "выключен — {}"},
        {"Period", "Период"},
        {"day", "день"},
        {"night", "ночь"},
        {"Camera", "Камера"},
        {"Motion", "Движение"},
        {"Person", "Человек"},
        {"here", "рядом"},
        {"gone · {}", "нет · {}"},
        {"{} s ago", "{} с назад"},
        {"{} min ago", "{} мин назад"},
        {"{} h ago", "{} ч назад"},
        {"connecting…", "подключение…"},
        {"Too dark in view: the camera may not see anyone. It needs IR illumination or light.",
         "В кадре слишком темно — камера может не видеть человека. Нужна ИК-подсветка или свет."},
        {"While the camera is unavailable, the screen stays on.",
         "Пока камера недоступна, экран остаётся включённым."},

        // Decision reasons (policy.cpp)
        {"control disabled", "управление выключено"},
        {"recent touch", "недавнее касание"},
        {"always on", "всегда включён"},
        {"off by schedule", "выключен по расписанию"},
        {"camera unavailable", "камера недоступна"},
        {"someone is here", "человек рядом"},
        {"nobody around", "никого нет"},

        // Settings
        {"Control", "Управление"},
        {"Manage the screen", "Управлять экраном"},
        {"Schedule", "Расписание"},
        {"Day starts", "День начинается"},
        {"Screen by day", "Днём экран"},
        {"Night starts", "Ночь начинается"},
        {"Screen at night", "Ночью экран"},
        {"Always on", "Всегда включён"},
        {"Off", "Выключен"},
        {"Auto (first found)", "Авто (первая найденная)"},
        {"Not connected: {}", "Не подключена: {}"},
        {"Check every", "Проверять каждые"},
        {"s", "с"},
        {"Sensitivity", "Чувствительность"},
        {"Low", "Низкая"},
        {"Medium", "Средняя"},
        {"High", "Высокая"},
        {"Keep on after leaving", "Не гасить после ухода"},
        {"Find cameras again", "Найти камеры заново"},
        {"Touch", "Касание"},
        {"Screen after a touch", "Экран после касания"},
        {"The camera records and stores nothing: one frame per interval is analysed in memory for "
         "motion only. The camera is used only during “Camera” periods; with checks every 5 s or "
         "less often it is switched on for about a second per check.",
         "Камера ничего не снимает и не сохраняет: кадр раз в интервал анализируется в памяти "
         "только на наличие движения. Камера используется лишь в периоды режима «По камере»; при "
         "проверке раз в 5 с и реже она включается примерно на секунду на каждую проверку."},

        // Camera errors (camera_v4l2.cpp, watcher.cpp)
        {"no camera found", "камера не найдена"},
        {"cannot open: {}", "не открывается: {}"},
        {"device cannot capture video", "устройство не умеет захват видео"},
        {"no uncompressed format (needs YUYV/GREY/NV12)", "нет несжатого формата (нужен YUYV/GREY/NV12)"},
        {"cannot set format: {}", "не удалось задать формат: {}"},
        {"cannot allocate buffers: {}", "не удалось выделить буферы: {}"},
        {"cannot start streaming: {}", "не удалось запустить поток: {}"},
        {"camera is not open", "камера не открыта"},
        {"read error: {}", "ошибка чтения: {}"},
        {"camera delivers no frames", "камера не отдаёт кадры"},
        {"corrupt frame", "кадр повреждён"},
    };
    return table;
}

}  // namespace

void register_translations(facet::i18n::Catalog& catalog) { catalog.add("ru", ru()); }

}  // namespace dp
