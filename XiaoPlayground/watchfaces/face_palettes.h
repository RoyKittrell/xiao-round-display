#pragma once
// Colour roles are changed before rasterisation. Antialiasing therefore mixes
// the new ink with the new dial, rather than recolouring already blended pixels.
// Each primitive does one palette lookup, not one lookup per output pixel.
namespace WatchFace { namespace Palettes {
inline uint16_t shade(uint16_t c,int amount){
 return rgb(((c>>11)*255/31)*amount/255,(((c>>5)&63)*255/63)*amount/255,(c&31)*255/31*amount/255);
}
inline uint16_t bigBenDark(uint16_t c){
 if(c==BigBen::Glass)return rgb(10,16,25);
 if(c==BigBen::Ink)return rgb(225,216,191);
 if(c==BigBen::Iron)return rgb(110,125,140);
 if(c==BigBen::Gold)return rgb(151,124,65);
 if(c==rgb(188,188,179))return rgb(42,52,65);
 if(c==rgb(99,105,107))return rgb(153,166,173);
 if(c==rgb(72,78,81)||c==rgb(76,83,86))return rgb(126,141,151);
 if(c==rgb(43,48,50))return rgb(99,115,131);
 return shade(c,190);
}
inline uint16_t stMarksDark(uint16_t c){
 if(c==StMarks::Stone)return rgb(13,22,35);
 if(c==StMarks::Ink)return rgb(211,204,177);
 if(c==StMarks::Blue)return rgb(7,13,40);
 if(c==StMarks::Gold)return rgb(180,146,80);
 if(c==rgb(235,229,198))return rgb(203,204,191);
 return shade(c,185);
}
inline uint16_t pragueLight(uint16_t c){
 if(c==Prague::Ink)return rgb(244,233,211);
 if(c==Prague::Gold)return rgb(111,78,28);
 if(c==Prague::Blue)return rgb(166,205,207);
 if(c==Prague::Rust)return rgb(218,177,141);
 if(c==Prague::Night)return rgb(40,61,80);
 if(c==rgb(233,230,205))return rgb(28,48,63);
 if(c==rgb(178,151,78)||c==rgb(166,145,80))return rgb(127,102,49);
 return c;
}
inline uint16_t pragueDark(uint16_t c){
 if(c==Prague::Ink)return rgb(7,14,23);
 if(c==Prague::Gold)return rgb(185,155,83);
 if(c==Prague::Blue)return rgb(13,43,59);
 if(c==Prague::Rust)return rgb(67,33,27);
 if(c==Prague::Night)return rgb(5,10,20);
 if(c==rgb(233,230,205))return rgb(215,205,169);
 return shade(c,192);
}
inline uint16_t conciergerieDark(uint16_t c){
 if(c==Conciergerie::Royal)return rgb(7,15,35);
 if(c==Conciergerie::Ivory)return rgb(12,18,30);
 if(c==Conciergerie::Ink)return rgb(213,205,178);
 if(c==Conciergerie::Gold)return rgb(179,145,77);
 if(c==rgb(219,180,85))return rgb(65,51,31);
 if(c==rgb(238,205,127))return rgb(127,99,49);
 return shade(c,185);
}
inline uint16_t zytgloggeLight(uint16_t c){
 if(c==Zytglogge::Red)return rgb(229,216,187);
 if(c==Zytglogge::Black)return rgb(244,233,210);
 if(c==Zytglogge::Gold)return rgb(107,75,28);
 if(c==Zytglogge::Sky)return rgb(159,192,189);
 if(c==rgb(240,218,153))return rgb(90,64,28);
 return c;
}
inline uint16_t zytgloggeDark(uint16_t c){
 if(c==Zytglogge::Red)return rgb(47,14,24);
 if(c==Zytglogge::Black)return rgb(6,13,22);
 if(c==Zytglogge::Sky)return rgb(11,34,47);
 if(c==Zytglogge::Gold)return rgb(181,149,82);
 return shade(c,195);
}
inline uint16_t spasskayaLight(uint16_t c){
 if(c==Spasskaya::Dark)return rgb(245,237,217);
 if(c==Spasskaya::Gold)return rgb(112,77,29);
 if(c==Spasskaya::Bronze)return rgb(190,151,83);
 return rgb(174,132,58);
}
inline uint16_t spasskayaDark(uint16_t c){
 if(c==Spasskaya::Dark)return rgb(5,9,15);
 if(c==Spasskaya::Gold)return rgb(194,160,79);
 if(c==Spasskaya::Bronze)return rgb(86,59,27);
 return shade(c,205);
}
inline uint16_t grandCentralDark(uint16_t c){
 if(c==GrandCentral::Ink)return rgb(222,207,171);
 if(c==GrandCentral::Brass)return rgb(145,110,46);
 int r=(c>>11)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
 if(r>=232&&g>=208&&b>=160)return rgb(13+(r-232)/3,17+(g-208)/3,22+(b-160)/4);
 if(c==rgb(163,138,92))return rgb(63,52,37);
 return shade(c,175);
}
inline uint16_t aberdeenDark(uint16_t c){
 if(c==Aberdeen::Glass)return rgb(182,127,56);
 if(c==Aberdeen::Ink)return rgb(22,17,10);
 if(c==Aberdeen::Lead)return rgb(85,53,23);
 if(c==Aberdeen::Gold)return rgb(195,143,60);
 if(c==Aberdeen::Rim)return rgb(8,13,15);
 if(c==Aberdeen::RimLight)return rgb(72,70,57);
 if(c==Aberdeen::Facet)return rgb(111,85,46);
 return shade(c,175);
}
inline uint16_t royalHollowayDark(uint16_t c){
 if(c==RoyalHolloway::Glass)return rgb(9,23,32);
 if(c==RoyalHolloway::Ink)return rgb(216,220,210);
 if(c==RoyalHolloway::Gold)return rgb(169,143,87);
 if(c==RoyalHolloway::Plate)return rgb(4,10,15);
 if(c==RoyalHolloway::HandColour)return rgb(216,220,203);
 if(c==RoyalHolloway::Facet)return rgb(135,159,157);
 return shade(c,185);
}
inline uint16_t sultanDark(uint16_t c){
 if(c==SultanAbdulSamad::Glass)return rgb(15,23,24);
 if(c==SultanAbdulSamad::Ink)return rgb(213,205,178);
 if(c==SultanAbdulSamad::Copper)return rgb(148,100,64);
 if(c==SultanAbdulSamad::Lead)return rgb(55,66,63);
 if(c==rgb(195,194,180))return rgb(40,50,47);
 if(c==rgb(206,167,119))return rgb(102,75,46);
 if(c==rgb(100,104,98))return rgb(128,142,136);
 return shade(c,180);
}
} }
