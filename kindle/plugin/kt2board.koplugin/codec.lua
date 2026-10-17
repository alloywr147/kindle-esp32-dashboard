local JSON=require('json')
local M={}
function M.decode(raw)
    local ok,value=pcall(JSON.decode,raw,{others={null=false,undefined=false}})
    if not ok then return nil,'JSON 格式错误' end
    return value
end
return M
