-- Experimental native KOReader widget. The ESP firmware is unchanged.
local InputContainer=require('ui/widget/container/inputcontainer')
local Geom=require('ui/geometry')
local Device=require('device')
local UIManager=require('ui/uimanager')
local Font=require('ui/font')
local TextBoxWidget=require('ui/widget/textboxwidget')
local BB=require('ffi/blitbuffer')
local JSON=require('json')
local http=require('socket.http')
local ltn12=require('ltn12')
local socket=require('socket')
local model=dofile('plugins/kt2board.koplugin/model.lua')
local codec=dofile('plugins/kt2board.koplugin/codec.lua')
local checkbox=dofile('plugins/kt2board.koplugin/checkbox.lua')
local Board=InputContainer:extend{is_always_active=true}
local TASK_TOP,TASK_ROW_HEIGHT,TASK_PAGE_SIZE=352,78,4
local function readJson(path)
    local f=io.open(path,'r');if not f then return nil end
    local raw=f:read('*a');f:close();return codec.decode(raw)
end
function Board:init()
    self.dimen=Geom:new{x=0,y=0,w=Device.screen:getWidth(),h=Device.screen:getHeight()}
    self.page,self.mode,self.status,self.failures=1,'today','正在连接 ESP…',0
    self.config=assert(readJson('kt2-config.json'),'Missing private KT2 config')
    assert(type(self.config.base_url)=='string' and self.config.base_url:match('^http://[%w%.%-]+:?%d*$'),'Set base_url to the ESP HTTP origin without a trailing slash')
    self.poll_seconds=math.max(5,tonumber(self.config.poll_seconds) or 5)
    self.state=readJson('kt2-last-state.json')
    if self.state and not model.validate(self.state) then self.state=nil end
    if self.state then self.status='离线缓存 · 等待同步' end
    self.rows={};self.nextTap=0;self.busy=false;self.closed=false
    self:registerTouchZones{{id='kt2_board_tap',ges='tap',screen_zone={ratio_x=0,ratio_y=0,ratio_w=1,ratio_h=1},handler=function(ges) return self:tap(ges.pos.x,ges.pos.y) end}}
    self.poller=function()
        if self.closed then return end
        self:poll()
        UIManager:scheduleIn(self.failures>0 and math.min(60,5*2^math.min(self.failures,4)) or self.poll_seconds,self.poller)
    end
    self.finish=function() self:exitBoard() end
    -- In-device verification hook, armed only by a local test request file.
    self.commander=function()
        if self.closed then return end
        local f=io.open('kt2-test-command.json','r')
        if f then
            local raw=f:read('*a');f:close();os.remove('kt2-test-command.json')
            local ok,c=pcall(JSON.decode,raw)
            if ok and c.action=='tap' then self:tap(tonumber(c.x) or -1,tonumber(c.y) or -1)
            elseif ok and c.action=='offline' then self.testOffline=c.enabled==true;self:reload()
            elseif ok and c.action=='exit' then self:exitBoard();return end
        end
        UIManager:scheduleIn(1,self.commander)
    end
end
function Board:request(path,method,data)
    if self.testOffline then return nil,'模拟网络中断' end
    local body=data and JSON.encode(data) or nil
    local result,bytes={},0
    local old=http.TIMEOUT;http.TIMEOUT=3
    local ok,_,code=pcall(http.request,{url=self.config.base_url..path,method=method or 'GET',
        headers={['Content-Type']='application/json',
                 ['Content-Length']=body and tostring(#body) or '0'},
        source=body and ltn12.source.string(body) or nil,
        sink=function(chunk)
            if chunk then bytes=bytes+#chunk;if bytes>65536 then return nil,'response too large' end;result[#result+1]=chunk end
            return 1
        end})
    http.TIMEOUT=old
    if not ok or tonumber(code)~=200 then return nil,'连接失败或未获确认' end
    local value=codec.decode(table.concat(result));if not value then return nil,'响应格式错误' end
    return value
end
function Board:cache(s)
    local f=io.open('kt2-last-state.tmp','w')
    if f then f:write(JSON.encode(s));f:close();os.rename('kt2-last-state.tmp','kt2-last-state.json') end
end
function Board:reload()
    local s=self:request('/api/state')
    local valid=s and model.validate(s)
    if not valid then
        self.failures=self.failures+1;self.status='连接失败 · 保留上次画面 · 未同步'
        UIManager:setDirty(self,'ui');print('KT2_SYNC_FAILED '..self.failures);return false
    end
    self.state=s;self.failures=0;self.status='已同步 '..os.date('%H:%M:%S');self:cache(s)
    if s.settings and s.settings.mode then self.mode=s.settings.mode end
    UIManager:setDirty(self,'ui');print('KT2_SYNC_OK revision='..tostring(s.revision)..' tasks='..#s.tasks)
    return true
end
function Board:poll()
    if self.busy then return end
    if not self.state or self.failures>0 then self:reload();return end
    local v=self:request('/api/version')
    if not v then
        self.failures=self.failures+1;self.status='离线 · 保留上次画面';UIManager:setDirty(self,'ui');return
    end
    local version=model.refreshVersion(v.version)
    if not version then
        self.failures=self.failures+1;self.status='版本响应异常 · 保留上次画面';UIManager:setDirty(self,'ui');return
    end
    if self.version~=version then self.version=version;self:reload() end
end
function Board:toggle(id)
    if self.busy or not self.state then return end
    local path,data=model.change(self.state,id);if not path then return end
    self.busy=true
    local reply=self:request(path,'PATCH',data)
    if not reply or reply.saved~=true then
        self.status='操作未获确认，请同步后查看';self.failures=self.failures+1
        UIManager:setDirty(self,'ui');print('KT2_SAVE_UNCONFIRMED id='..id)
    else
        print('KT2_SAVE_ACK id='..id..' done='..tostring(data.done))
        if not self:reload() then self.status='已提交，等待重新连接确认';UIManager:setDirty(self,'ui') end
    end
    self.busy=false
end
function Board:setMode(mode)
    local reply=self:request('/api/settings','PUT',{mode=mode})
    if reply and reply.saved then self.page=1;self:reload()
    else self.status='页面切换未同步';UIManager:setDirty(self,'ui') end
end
function Board:tap(x,y)
    if self.closed or socket.gettime()<self.nextTap then return true end
    self.nextTap=socket.gettime()+0.5
    print('KT2_TAP x='..x..' y='..y)
    if y>=740 then
        if x<150 then self:setMode('today') elseif x<300 then self:setMode('almanac')
        elseif x<450 then self:setMode('weather') else self:exitBoard() end
    elseif y>=670 and y<710 and self.mode=='today' then
        self.page=self.page+(x<300 and -1 or 1);UIManager:setDirty(self,'ui')
    elseif self.mode=='today' and y>=TASK_TOP and y<TASK_TOP+TASK_ROW_HEIGHT*TASK_PAGE_SIZE and x>=12 and x<=588 then
        local row=math.floor((y-TASK_TOP)/TASK_ROW_HEIGHT)+1
        if self.rows[row] then self:toggle(self.rows[row].id) end
    end
    return true
end
function Board:onShow()
    UIManager:setDirty(self,'full')
    UIManager:scheduleIn(0.1,function() self:reload();self.poller();if self.config.enable_test_commands then self.commander() end end)
    local duration=tonumber(self.config.test_duration) or 0
    if duration>0 then UIManager:scheduleIn(duration,self.finish) end
    print(duration>0 and ('KT2_RUN_LIMIT '..duration) or 'KT2_RUN_MODE continuous')
    print('KT2_BOARD_OPEN '..self.dimen.w..'x'..self.dimen.h..' build=20261017-7-public')
end
function Board:exitBoard()
    if self.closed then return end
    self.closed=true;UIManager:unschedule(self.poller);UIManager:unschedule(self.commander);UIManager:unschedule(self.finish)
    UIManager:close(self);UIManager:quit();print('KT2_BOARD_EXIT')
end
local function text(bb,x,y,w,h,value,size,bold)
    local widget=TextBoxWidget:new{text=tostring(value or ''),face=Font:getFace('cfont',size),width=w,height=h,
        height_adjust=false,padding=0,bold=bold or false,line_height=0.18}
    widget:paintTo(bb,x,y);widget:free()
end
function Board:paintTo(bb,x,y)
    self.dimen.x,self.dimen.y=x,y
    bb:paintRect(x,y,self.dimen.w,self.dimen.h,BB.COLOR_WHITE)
    local s=self.state
    text(bb,20,16,560,45,s and s.date or 'KT2 日程板',30,true)
    local a=s and s.almanac or {}
    text(bb,20,64,560,42,(a.weekday or '')..'  '..(a.lunar_date or '等待 ESP 数据'),22)
    bb:paintRect(20,112,560,2,BB.COLOR_BLACK)
    if self.mode=='today' then
        local w=s and s.weather or {};local c=w.current
        text(bb,20,130,560,48,'苏州  '..(c and (tostring(c.temperature)..'℃  '..tostring(c.condition)) or '天气暂不可用'),30)
        local d=s and model.day(s,0)
        text(bb,20,184,560,35,d and ('今日 '..d.low..'～'..d.high..'℃  降水 '..d.rain..'%') or '等待有效天气',21)
        text(bb,20,230,560,32,'宜  '..(a.available and table.concat(a.yi or {},' · ') or '暂无黄历数据'),21)
        text(bb,20,266,560,32,'忌  '..(a.available and table.concat(a.ji or {},' · ') or '暂无黄历数据'),21)
        text(bb,20,302,560,32,a.available and ('冲煞  '..(a.chong_sha or '暂无')) or '冲煞  暂无黄历数据',21)
        bb:paintRect(20,342,560,1,BB.COLOR_GRAY)
        self.rows,self.page,self.total=model.page(s or {tasks={}},self.page,TASK_PAGE_SIZE)
        for i,t in ipairs(self.rows) do
            local top=TASK_TOP+(i-1)*TASK_ROW_HEIGHT
            checkbox.draw(bb,20,top+8,t.done)
            text(bb,84,top+2,492,68,model.short(t.text,34),25)
            bb:paintRect(20,top+TASK_ROW_HEIGHT-5,560,1,BB.COLOR_GRAY)
        end
        if #self.rows==0 then text(bb,20,TASK_TOP+14,560,80,'还没有待办，请在手机添加。',26) end
        text(bb,20,674,560,34,'上一页           '..self.page..' / '..self.total..'           下一页',21)
    elseif self.mode=='almanac' then
        text(bb,20,132,560,42,'今日黄历',30,true)
        if a.available then
            text(bb,20,193,560,80,(a.ganzhi or '')..'\n'..(a.seasonal_note or ''),24)
            text(bb,20,295,560,120,'宜  '..table.concat(a.yi or {},' · '),27)
            text(bb,20,430,560,120,'忌  '..table.concat(a.ji or {},' · '),27)
            text(bb,20,568,560,70,a.chong_sha or '',22)
            text(bb,20,650,560,35,'传统民俗，仅供参考',20)
        else text(bb,20,210,560,100,'日期未就绪或黄历数据不可用',25) end
    else
        text(bb,20,130,560,45,'苏州 · 七日天气预报',29,true)
        for i=0,6 do
            local top=198+i*65;local d=s and model.day(s,i)
            local date=s and model.shiftDate(s.date,i) or ''
            text(bb,20,top,108,45,i==0 and '今天' or i==1 and '明天' or (date or ''):sub(6),23)
            text(bb,132,top,155,45,d and d.condition or '暂无',22)
            text(bb,292,top,175,45,d and (d.low..'/'..d.high..'℃') or '--',23)
            text(bb,475,top,100,45,d and (d.rain..'%') or '--',22)
            bb:paintRect(20,top+54,560,1,BB.COLOR_GRAY)
        end
    end
    local w=s and s.weather or {}
    text(bb,20,712,560,25,self.status..(w.status=='stale' and ' · 天气延迟' or ''),16)
    bb:paintRect(0,740,600,2,BB.COLOR_BLACK)
    for i,label in ipairs({'待办','黄历','七日天气','退出'}) do text(bb,(i-1)*150+18,756,135,40,label,23,true) end
end
return Board
