# xf86-video-armsoc depends on xf86driproto, but *proto repositories 
# (including xf86driproto) have been moved into a single repository called
# xorgproto. 

DEPENDS_remove = "xf86driproto"
DEPENDS += "xorgproto"
