local BB=require('ffi/blitbuffer')
local checkbox=dofile('plugins/kt2board.koplugin/checkbox.lua')
for _,done in ipairs({false,true}) do
    local bb=BB.new(100,100,BB.TYPE_BB8)
    bb:paintRect(0,0,100,100,BB.COLOR_WHITE)
    checkbox.draw(bb,20,20,done)
    for i=0,47 do
        assert(bb:getPixel(20+i,20):getColor8().a==0,'top border erased')
        assert(bb:getPixel(20+i,67):getColor8().a==0,'bottom border erased')
        assert(bb:getPixel(20,20+i):getColor8().a==0,'left border erased')
        assert(bb:getPixel(67,20+i):getColor8().a==0,'right border erased')
    end
    assert(bb:getPixel(75,13):getColor8().a==(done and 0 or 255),'overflow check endpoint')
    bb:free()
end
print('PASS checked and unchecked: four complete borders; overflow check endpoint')
