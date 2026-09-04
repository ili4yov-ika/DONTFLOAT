-- Сценарий для настоящего REAPER: две дорожки с аудио и VST3 DONTFLOAT,
-- затем правки клипов, которые ARA обязана донести до плагина.
--
-- Ставится как __startup.lua и запускается самим REAPER при старте. Пути и
-- тайминги подставляет тест (reaper_ara_integration_test), плейсхолдеры вида
-- @@ИМЯ@@ он заменяет перед копированием.
--
-- Отчёт пишется построчно в @@REPORT@@: тест читает его после выхода REAPER.
-- Всё, что скрипт делает, он туда же и записывает — если REAPER упадёт на
-- полпути, по последней строке видно, на каком шаге.
--
-- Важное про Undo_BeginBlock/EndBlock вокруг каждой правки: без фиксации
-- транзакции REAPER не рассылает ARA-уведомления, и до плагина не доходит
-- ничего — проверено, дневник оставался пустым.

-- Файлы намеренно разные: один и тот же REAPER заводит как единственный
-- ARA-источник, и тогда референсным нотам просто неоткуда взяться —
-- сосед у источника один, он сам
local AUDIO      = [[@@AUDIO@@]]
local AUDIO2     = [[@@AUDIO2@@]]
local REPORT     = [[@@REPORT@@]]
-- На второй дорожке редакция Pitcher: референсные ноты соседа подтягивает
-- именно пианорольный редактор, а в полной редакции он на отдельной вкладке
-- и сам собой не открывается
local FX_NAME    = [[@@FX_NAME@@]]
local FX_NAME2   = [[@@FX_NAME2@@]]
local SETTLE_SEC = tonumber("@@SETTLE_SEC@@") or 4
local PROJECT    = [[@@PROJECT@@]]

-- Дорожки и индексы плагинов: окна редакторов нужно закрыть перед выходом,
-- иначе REAPER остаётся с открытым окном плагина и не закрывается
local tracks = {}
local takes = {}
local fxIndex = {}

local report = io.open(REPORT, "w")
local function say(line)
    if report then
        report:write(line .. "\n")
        report:flush()
    end
end

local function closeEditors()
    for i = 1, #tracks do
        if fxIndex[i] and takes[i] then
            reaper.TakeFX_Show(takes[i], fxIndex[i], 2) -- 2 = скрыть плавающее окно
        end
    end
end

local function finish(status)
    closeEditors()
    -- Сохраняем перед выходом: иначе REAPER спросит про несохранённый проект
    -- и будет ждать человека
    reaper.Main_SaveProjectEx(0, PROJECT, 0)
    say("status=" .. status)
    if report then report:close() end
    reaper.Main_OnCommand(40004, 0) -- File: Quit REAPER
end

say("version=" .. tostring(reaper.GetAppVersion()))
say("audio=" .. AUDIO)

-- ---------------------------------------------------------------- проект ---
while reaper.CountTracks(0) > 0 do
    reaper.DeleteTrack(reaper.GetTrack(0, 0))
end
reaper.SetEditCurPos(0, false, false)

-- ------------------------------------------------- две дорожки с аудио ---
for i = 1, 2 do
    reaper.InsertTrackAtIndex(i - 1, true)
    local track = reaper.GetTrack(0, i - 1)
    tracks[i] = track
    reaper.SetOnlyTrackSelected(track)
    -- Вторую дорожку начинаем с секунды: клипы не должны совпасть по позиции,
    -- иначе перенос одного не отличить от переноса другого
    reaper.SetEditCurPos((i - 1) * 1.0, false, false)
    reaper.InsertMedia(i == 1 and AUDIO or AUDIO2, 0)
end

say("tracks=" .. reaper.CountTracks(0))
if reaper.CountTracks(0) < 2 then
    finish("no-tracks")
    return
end

-- ------------------------------------------------------------- плагины ---
local added = 0
for i = 1, 2 do
    -- Плагин вешаем на КЛИП, а не на дорожку: только тогда REAPER назначает
    -- ARA-рендереру регионы. На дорожке их ноль, и экземпляр не знает ни
    -- своего звука, ни своей разметки
    local item = reaper.GetTrackMediaItem(tracks[i], 0)
    local take = item and reaper.GetActiveTake(item)
    local fx = take and reaper.TakeFX_AddByName(take, i == 1 and FX_NAME or FX_NAME2, -1) or -1
    if fx >= 0 then
        added = added + 1
        takes[i] = take
        fxIndex[i] = fx
        local _, name = reaper.TakeFX_GetFXName(take, fx)
        say("fx" .. i .. "=" .. tostring(name))
        -- Окно редактора: ноты на общую доску выкладывает именно он
        reaper.TakeFX_Show(take, fx, 3)
    else
        say("fx" .. i .. "=FAILED")
    end
end
say("fx_added=" .. added)
if added < 2 then
    finish("no-fx")
    return
end

-- --------------------------------------------------------------- правки ---
-- Между фазами возвращаем управление REAPER: иначе он не успеет ни привязать
-- ARA, ни отработать колбэки.
local phase = 0
local waitUntil = reaper.time_precise() + SETTLE_SEC

local function itemOf(trackIndex)
    local track = tracks[trackIndex]
    if reaper.CountTrackMediaItems(track) == 0 then return nil end
    return reaper.GetTrackMediaItem(track, 0)
end

local function step()
    if reaper.time_precise() < waitUntil then
        reaper.defer(step)
        return
    end

    phase = phase + 1

    if phase == 1 then
        -- Нарезка: первый клип надвое
        local item = itemOf(1)
        if not item then finish("no-item-1") return end
        local pos = reaper.GetMediaItemInfo_Value(item, "D_POSITION")
        local len = reaper.GetMediaItemInfo_Value(item, "D_LENGTH")
        reaper.Undo_BeginBlock()
        reaper.SplitMediaItem(item, pos + len / 2)
        reaper.Undo_EndBlock("dontfloat test: split", -1)
        say("split_at=" .. string.format("%.3f", pos + len / 2))
        say("items_track1=" .. reaper.CountTrackMediaItems(tracks[1]))

    elseif phase == 2 then
        -- Перенос: вторую половину сдвигаем вправо
        local track = tracks[1]
        local count = reaper.CountTrackMediaItems(track)
        if count < 2 then finish("no-split") return end
        local item = reaper.GetTrackMediaItem(track, count - 1)
        local pos = reaper.GetMediaItemInfo_Value(item, "D_POSITION")
        reaper.Undo_BeginBlock()
        reaper.SetMediaItemPosition(item, pos + 2.0, false)
        reaper.Undo_EndBlock("dontfloat test: move", -1)
        say("moved_to=" .. string.format("%.3f", pos + 2.0))

    elseif phase == 3 then
        -- Растяжение: клип второй дорожки удлиняем в полтора раза
        local item = itemOf(2)
        if not item then finish("no-item-2") return end
        local take = reaper.GetActiveTake(item)
        local len = reaper.GetMediaItemInfo_Value(item, "D_LENGTH")
        reaper.Undo_BeginBlock()
        if take then
            local rate = reaper.GetMediaItemTakeInfo_Value(take, "D_PLAYRATE")
            reaper.SetMediaItemTakeInfo_Value(take, "D_PLAYRATE", rate / 1.5)
        end
        reaper.SetMediaItemLength(item, len * 1.5, false)
        reaper.Undo_EndBlock("dontfloat test: stretch", -1)
        say("stretched_to=" .. string.format("%.3f", len * 1.5))

    elseif phase == 4 then
        -- Сжатие: тот же клип обратно и ещё короче
        local item = itemOf(2)
        if not item then finish("no-item-2") return end
        local take = reaper.GetActiveTake(item)
        local len = reaper.GetMediaItemInfo_Value(item, "D_LENGTH")
        reaper.Undo_BeginBlock()
        if take then
            local rate = reaper.GetMediaItemTakeInfo_Value(take, "D_PLAYRATE")
            reaper.SetMediaItemTakeInfo_Value(take, "D_PLAYRATE", rate * 2.0)
        end
        reaper.SetMediaItemLength(item, len / 2.0, false)
        reaper.Undo_EndBlock("dontfloat test: compress", -1)
        say("compressed_to=" .. string.format("%.3f", len / 2.0))

    elseif phase == 5 then
        -- Воспроизведение: под ARA захват выключен, и волна не должна
        -- обнуляться от тишины на входе
        reaper.SetEditCurPos(0, false, false)
        reaper.Main_OnCommand(1007, 0) -- Transport: Play
        say("playing")

    elseif phase == 6 then
        reaper.Main_OnCommand(1016, 0) -- Transport: Stop
        say("stopped")

    else
        say("items_track1_final=" .. reaper.CountTrackMediaItems(tracks[1]))
        say("items_track2_final=" .. reaper.CountTrackMediaItems(tracks[2]))
        finish("ok")
        return
    end

    reaper.UpdateArrange()
    waitUntil = reaper.time_precise() + SETTLE_SEC
    reaper.defer(step)
end

reaper.defer(step)
