-- Draw only black pixels so the check never erases the box border.
local BB=require('ffi/blitbuffer')
local checkbox={}
local function stroke(bb,x1,y1,x2,y2)
    local steps=math.max(math.abs(x2-x1),math.abs(y2-y1))
    for i=0,steps do
        local x=math.floor(x1+(x2-x1)*i/steps+0.5)
        local y=math.floor(y1+(y2-y1)*i/steps+0.5)
        bb:paintRect(x-2,y-2,5,5,BB.COLOR_BLACK)
    end
end
function checkbox.draw(bb,x,y,done)
    bb:paintBorder(x,y,48,48,3,BB.COLOR_BLACK)
    if done then
        stroke(bb,x+9,y+24,x+22,y+37)
        stroke(bb,x+22,y+37,x+55,y-7)
    end
end
return checkbox
