-- Pure data and layout helpers. No device, network or clock side effects.
local M = {}
local function numeric(v)
    return type(v)=='number' and v==v and v~=math.huge and v~=-math.huge
end
function M.validate(s)
    if type(s)~='table' or type(s.tasks)~='table' or #s.tasks>50 then return nil,'待办数据不完整' end
    if type(s.clock_valid)~='boolean' or type(s.date)~='string' then return nil,'日期数据不完整' end
    if s.clock_valid and not s.date:match('^%d%d%d%d%-%d%d%-%d%d$') then return nil,'日期格式错误' end
    local ids={}
    for _,t in ipairs(s.tasks) do
        if type(t)~='table' or type(t.id)~='number' or t.id<1 or t.id%1~=0 or ids[t.id]
          or type(t.text)~='string' or #t.text==0 or type(t.done)~='boolean' then return nil,'待办格式错误' end
        ids[t.id]=true
    end
    if type(s.almanac)~='table' or type(s.weather)~='table' then return nil,'黄历或天气数据缺失' end
    if s.almanac.available and (not s.clock_valid or s.almanac.date~=s.date) then return nil,'黄历日期不匹配' end
    for _,key in ipairs({'weekday','lunar_date','ganzhi','seasonal_note','chong_sha'}) do
        if s.almanac[key]~=nil and type(s.almanac[key])~='string' then return nil,'黄历文字格式错误' end
    end
    for _,key in ipairs({'yi','ji'}) do
        local value=s.almanac[key]
        if value~=nil then
            if type(value)~='table' or #value>4 then return nil,'黄历条目格式错误' end
            for _,entry in ipairs(value) do if type(entry)~='string' then return nil,'黄历条目格式错误' end end
        end
    end
    local weather=s.weather
    if weather.status~='live' and weather.status~='stale' and weather.status~='unavailable' then return nil,'天气状态错误' end
    if weather.current~=nil then
        if type(weather.current)~='table' or not numeric(weather.current.temperature) or type(weather.current.condition)~='string' then return nil,'当前天气格式错误' end
    end
    if weather.days~=nil then
        if type(weather.days)~='table' or #weather.days>7 then return nil,'预报格式错误' end
        for _,day in ipairs(weather.days) do
            if type(day)~='table' or type(day.date)~='string' or not day.date:match('^%d%d%d%d%-%d%d%-%d%d$')
              or not numeric(day.low) or not numeric(day.high) or not numeric(day.rain) or type(day.condition)~='string' then return nil,'预报明细缺失' end
        end
    end
    if s.settings and (type(s.settings)~='table' or (s.settings.mode~='today' and s.settings.mode~='almanac' and s.settings.mode~='weather')) then return nil,'页面设置错误' end
    return s
end
function M.change(s,id)
    for _,t in ipairs(s.tasks) do
        if t.id==id then return '/api/tasks/'..id,{done=not t.done} end
    end
end
function M.page(s,page,size)
    local total=math.max(1,math.ceil(#s.tasks/size))
    page=math.max(1,math.min(page,total));local result={}
    for i=(page-1)*size+1,math.min(page*size,#s.tasks) do result[#result+1]=s.tasks[i] end
    return result,page,total
end
local function monthDays(y,m)
    if m==2 then return ((y%4==0 and y%100~=0) or y%400==0) and 29 or 28 end
    return ({31,28,31,30,31,30,31,31,30,31,30,31})[m]
end
function M.shiftDate(date,offset)
    local y,m,d=date:match('^(%d%d%d%d)%-(%d%d)%-(%d%d)$')
    y,m,d=tonumber(y),tonumber(m),tonumber(d)
    if not y or m<1 or m>12 or d<1 or d>monthDays(y,m) or offset<0 or offset>7 then return nil end
    for _=1,offset do d=d+1;if d>monthDays(y,m) then d=1;m=m+1;if m>12 then m=1;y=y+1 end end end
    return string.format('%04d-%02d-%02d',y,m,d)
end
function M.day(s,offset)
    if not s.clock_valid then return nil end
    local date=M.shiftDate(s.date,offset)
    for _,day in ipairs(s.weather.days or {}) do if day.date==date then return day end end
end
function M.short(text,n)
    local count=0
    for i=1,#text do
        local b=text:byte(i)
        if b<128 or b>=192 then count=count+1;if count>n then return text:sub(1,i-1)..'…' end end
    end
    return text
end
-- ESP versions carry a five-minute UTC bucket. Collapse it to an hour;
-- Suzhou midnight is an hour boundary, so dates still roll over promptly.
function M.refreshVersion(value)
    if type(value)~='string' then return nil end
    local revision,bucket,pending=value:match('^(%d+):(%d+):(%d+)$')
    if not revision then return nil end
    return revision..':'..math.floor(tonumber(bucket)/12)..':'..pending
end
return M
