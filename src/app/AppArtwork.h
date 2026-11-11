#pragma once
#include <QImage>
#include <QColor>
#include <QMap>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace AppArtwork {
struct Cleanup {QImage image;bool accepted=false;QString reason;};
// Classification uses quantized colours; final RGB is reconstructed from the
// original samples. Work is bounded to a 512-square icon, never a media image.
inline Cleanup cleanBackground(const QImage &source,int strength=1,const QColor &chosen={})
{
    Cleanup result{source,false,QStringLiteral("Keep Original: no reliable uniform background was found.")};
    if(source.isNull() || source.width()>512 || source.height()>512 || source.width()<8 || source.height()<8 || strength<0 || strength>2)return result;
    auto image=source.convertToFormat(QImage::Format_ARGB32);const int w=image.width(),h=image.height(),count=w*h;
    int transparent=0;for(int y=0;y<h;++y)for(int x=0;x<w;++x)transparent+=qAlpha(image.pixel(x,y))<16;
    if(!chosen.isValid() && transparent>count*.15){result.reason=QStringLiteral("Already transparent. Keep Original, or choose the backdrop explicitly.");return result;}
    const auto distance=[](QRgb a,QRgb b){return std::max({std::abs(qRed(a)-qRed(b)),std::abs(qGreen(a)-qGreen(b)),std::abs(qBlue(a)-qBlue(b))});};
    QVector<QRgb> samples;QMap<int,int> palette;
    const int band=std::max(1,std::min(w,h)/32);
    auto boundary=[&](int x,int y){return x<band || y<band || x>=w-band || y>=h-band || qAlpha(image.pixel(x-1,y))<16 || qAlpha(image.pixel(x+1,y))<16 || qAlpha(image.pixel(x,y-1))<16 || qAlpha(image.pixel(x,y+1))<16;};
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){const auto p=image.pixel(x,y);if(qAlpha(p)<248 || !boundary(x,y))continue;samples.append(p);++palette[((qRed(p)/16)<<8)|((qGreen(p)/16)<<4)|(qBlue(p)/16)];}
    if(samples.size()<16)return result;
    QRgb backdrop=chosen.rgba();
    if(!chosen.isValid()){
        int best=-1,key=0;for(auto it=palette.cbegin();it!=palette.cend();++it)if(it.value()>best){best=it.value();key=it.key();}
        qint64 r=0,g=0,b=0,n=0;for(auto p:samples)if((((qRed(p)/16)<<8)|((qGreen(p)/16)<<4)|(qBlue(p)/16))==key){r+=qRed(p);g+=qGreen(p);b+=qBlue(p);++n;}
        if(!n)return result;backdrop=qRgb(r/n,g/n,b/n);
    }
    const int tolerance=QList<int>{16,28,42}[strength],soft=tolerance+24;
    int support=0;for(auto p:samples)if(distance(p,backdrop)<=tolerance)++support;
    if(!chosen.isValid() && support<samples.size()*.7)return result;
    QVector<int> pending;QVector<bool> visited(count,false);
    auto enqueue=[&](int x,int y){const int i=y*w+x;if(visited[i])return;const auto p=image.pixel(x,y);if(qAlpha(p)<8 || distance(p,backdrop)>soft)return;visited[i]=true;pending.append(i);};
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)if(boundary(x,y))enqueue(x,y);
    for(int i=0;i<pending.size();++i){const int x=pending[i]%w,y=pending[i]/w;if(x)enqueue(x-1,y);if(x+1<w)enqueue(x+1,y);if(y)enqueue(x,y-1);if(y+1<h)enqueue(x,y+1);}
    int opaque=0;for(int y=0;y<h;++y)for(int x=0;x<w;++x)opaque+=qAlpha(image.pixel(x,y))>8;
    if(pending.size()<opaque*.03 || pending.size()>opaque*.94){result.reason=QStringLiteral("Keep Original: cleanup would remove too little or too much artwork.");return result;}
    const auto linear=[](double v){v/=255.;return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    const auto srgb=[](double v){v=std::clamp(v,0.,1.);return qRound(255*(v<=.0031308?12.92*v:1.055*std::pow(v,1/2.4)-.055));};
    for(const int i:pending){const int x=i%w,y=i/w;const auto p=image.pixel(x,y);const double alpha=std::clamp(double(distance(p,backdrop)-tolerance)/(soft-tolerance),0.,1.);
        if(alpha<.05){image.setPixel(x,y,0);continue;}
        const auto recover=[&](int c,int bg){return srgb((linear(c)-(1-alpha)*linear(bg))/alpha);};
        image.setPixel(x,y,qRgba(recover(qRed(p),qRed(backdrop)),recover(qGreen(p),qGreen(backdrop)),recover(qBlue(p),qBlue(backdrop)),qRound(qAlpha(p)*alpha)));
    }
    // Reconstruct the adjacent matte transition without deleting internal
    // white lettering or small disconnected marks elsewhere in the artwork.
    const auto classified=image;
    for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x){const auto p=source.pixelColor(x,y);if(visited[y*w+x] || p.alpha()<248)continue;
        if(qAlpha(classified.pixel(x-1,y))>8 && qAlpha(classified.pixel(x+1,y))>8 && qAlpha(classified.pixel(x,y-1))>8 && qAlpha(classified.pixel(x,y+1))>8)continue;
        int fx=-1,fy=-1;for(int oy=-2;oy<=2;++oy)for(int ox=-2;ox<=2;++ox){const int nx=x+ox,ny=y+oy;if(nx<0 || ny<0 || nx>=w || ny>=h || visited[ny*w+nx])continue;if(distance(source.pixel(nx,ny),backdrop)>distance(p.rgba(),backdrop)+24){fx=nx;fy=ny;}}
        if(fx<0)continue;const auto foreground=source.pixel(fx,fy);double dot=0,length=0;
        for(int channel=0;channel<3;++channel){const int shift=16-8*channel;const double bg=linear((backdrop>>shift)&255),fg=linear((foreground>>shift)&255),value=linear((p.rgba()>>shift)&255);dot+=(value-bg)*(fg-bg);length+=(fg-bg)*(fg-bg);}
        const double alpha=length>.01?std::clamp(dot/length,0.,1.):1;
        if(alpha>.05 && alpha<.98){const auto recover=[&](int c,int bg){return srgb((linear(c)-(1-alpha)*linear(bg))/alpha);};image.setPixel(x,y,qRgba(recover(p.red(),qRed(backdrop)),recover(p.green(),qGreen(backdrop)),recover(p.blue(),qBlue(backdrop)),qRound(alpha*255)));}
    }
    result.image=image;result.accepted=true;result.reason=QStringLiteral("Clean background preview. Original colours and source are retained.");return result;
}
// A reviewed mask may turn declared internal colours into negative space.
// Colour foregrounds and sources are never modified by this conversion.
inline QImage inkMask(const QImage &source,const QColor &ink,const QList<QColor> &exclude)
{
    auto result=source.convertToFormat(QImage::Format_ARGB32);
    for(int y=0;y<result.height();++y)for(int x=0;x<result.width();++x){
        const auto p=result.pixelColor(x,y);double coverage=1;
        for(const auto &background:exclude){
            const double dr=ink.redF()-background.redF(),dg=ink.greenF()-background.greenF(),db=ink.blueF()-background.blueF();
            const double length=dr*dr+dg*dg+db*db;
            if(length>.01)coverage=std::min(coverage,std::clamp(((p.redF()-background.redF())*dr+(p.greenF()-background.greenF())*dg+(p.blueF()-background.blueF())*db)/length,0.,1.));
        }
        result.setPixelColor(x,y,QColor(255,255,255,qRound(p.alpha()*coverage)));
    }
    return result;
}
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
