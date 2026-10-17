local WidgetContainer = require('ui/widget/container/widgetcontainer')
local UIManager = require('ui/uimanager')
local Plugin = WidgetContainer:extend{name='kt2board',is_doc_only=false}
function Plugin:init()
    self.ui.menu:registerToMainMenu(self)
    local f=io.open('kt2-test-autostart','r')
    if f then f:close();os.remove('kt2-test-autostart');UIManager:scheduleIn(2,function() self:openBoard() end) end
end
function Plugin:addToMainMenu(items)
    items.kt2board={text='KT2 日程板',sorting_hint='more_tools',callback=function() self:openBoard() end}
end
function Plugin:openBoard()
    local Board=dofile('plugins/kt2board.koplugin/board.lua')
    UIManager:show(Board:new{})
end
return Plugin
