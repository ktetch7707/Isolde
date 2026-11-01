#pragma once
#include <QImage>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace AppArtwork {
// Remove only a uniform, opaque, edge-connected backdrop. Enclosed white
// lettering survives; existing transparency and ambiguous artwork are retained.
inline QImage cutout(const QImage &source)
{
    if(source.isNull() || source.width()>512 || source.height()>512)return source;
    auto image=source.convertToFormat(QImage::Format_ARGB32);
    const int w=image.width(),h=image.height();
    const auto background=image.pixel(0,0);
    auto matches=[&](QRgb pixel){return qAlpha(pixel)>248 && std::max({std::abs(qRed(pixel)-qRed(background)),std::abs(qGreen(pixel)-qGreen(background)),std::abs(qBlue(pixel)-qBlue(background))})<=18;};
    if(!matches(background) || !matches(image.pixel(w-1,0)) || !matches(image.pixel(0,h-1)) || !matches(image.pixel(w-1,h-1)))return source;
    QVector<int> pending;QVector<bool> visited(w*h,false);
    auto enqueue=[&](int x,int y){const int i=y*w+x;if(!visited[i]){visited[i]=true;if(matches(image.pixel(x,y)))pending.append(i);}};
    for(int x=0;x<w;++x){enqueue(x,0);enqueue(x,h-1);}
    for(int y=0;y<h;++y){enqueue(0,y);enqueue(w-1,y);}
    for(int i=0;i<pending.size();++i){const int x=pending[i]%w,y=pending[i]/w;
        if(x)enqueue(x-1,y);if(x+1<w)enqueue(x+1,y);if(y)enqueue(x,y-1);if(y+1<h)enqueue(x,y+1);
    }
    if(pending.size()>w*h*.95)return source;
    for(const int i:pending)image.setPixel(i%w,i/w,qRgba(0,0,0,0));
    return image;
}
}
