#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    dialog=new QFileDialog(this);
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    QStringList filters;
    filters<<"All images (*.jpg *.jpeg *.png *.gif *.bmp)"
           <<"JPEG images (*.jpg *.jpeg)"
           <<"PNG images (*.png)"
           <<"GIF images (*.gif)"
           <<"Bitmaps (*.bmp)";
    dialog->setNameFilters(filters);
    dialog->setDirectory(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    connect(dialog,SIGNAL(fileSelected(QString)),this,SLOT(dialogFileSelected(QString)));
    connect(ui->browseBtn,SIGNAL(clicked(bool)),this,SLOT(browseBtnClicked()));
    connect(ui->loadBtn,SIGNAL(clicked(bool)),this,SLOT(loadBtnClicked()));
    connect(ui->saveAsBtn,SIGNAL(clicked(bool)),this,SLOT(saveAsBtnClicked()));
    connect(ui->fitToWindowBtn,SIGNAL(clicked(bool)),this,SLOT(fitToWindow()));
    connect(ui->resetZoomBtn,SIGNAL(clicked(bool)),this,SLOT(resetZoom()));
    connect(ui->resetBtn,SIGNAL(clicked(bool)),this,SLOT(resetBtnClicked()));
    connect(ui->pixelateBtn,SIGNAL(clicked(bool)),this,SLOT(pixelateBtnClicked()));
    image=0;
    originalImageData=0;
    originalImageWidth=-1;
    originalImageHeight=-1;
    scene=new QGraphicsScene();
    pixmapItem=new QGraphicsPixmapItem();
    scene->addItem(pixmapItem);
    ui->graphicsView->setScene(scene);
}

MainWindow::~MainWindow()
{
    free(originalImageData);
    delete ui;
}

void MainWindow::browseBtnClicked()
{
    dialog->exec();
}

void MainWindow::loadBtnClicked()
{
    QString path=ui->pathBox->text();
    if(path.length()==0)
    {
        QMessageBox::critical(this,"Error","No file selected.");
        return;
    }
    QFile f(path);
    if(!f.exists())
    {
        QMessageBox::critical(this,"Error","The selected file does not exist.");
        return;
    }
    image=new QImage(path);
    if(image->isNull())
    {
        QMessageBox::critical(this,"Error","The selected file has an unsupported format.");
        return;
    }
    originalImageWidth=image->width();
    originalImageHeight=image->height();
    free(originalImageData);
    originalImageData=qImageToBitmapData(image);
    scene->setSceneRect(0,0,originalImageWidth,originalImageHeight);
    pixmapItem->setPixmap(QPixmap::fromImage(*image));
    ui->graphicsView->viewport()->update();
    fitToWindow();
}

void MainWindow::saveAsBtnClicked()
{
    if(originalImageData==0)
        return;
    QString path=QFileDialog::getSaveFileName(this,"Save as...",QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),"JPG image (*.jpg);;PNG image (*.png);;GIF image (*.gif);;Bitmap (*.bmp)");
    if(path=="")
        return;
    image->save(path,0,100);
}

void MainWindow::fitToWindow()
{
    if(image==0||image->isNull())
        return;
    int width=image->width();
    int height=image->height();
    QRect rect=ui->graphicsView->contentsRect();
    int availableWidth=rect.width()-ui->graphicsView->verticalScrollBar()->width();
    int availableHeight=rect.height()-ui->graphicsView->horizontalScrollBar()->height();
    if((width-availableWidth)>(height-availableHeight))
        ui->graphicsView->setZoomFactor((float)((float)availableWidth)/((float)width));
    else if(height>availableHeight)
        ui->graphicsView->setZoomFactor((float)((float)availableHeight)/((float)height));
    else
        ui->graphicsView->setZoomFactor(1.0);
}

void MainWindow::resetZoom()
{
    ui->graphicsView->setZoomFactor(1.0);
}

void MainWindow::dialogFileSelected(QString path)
{
    ui->pathBox->setText(path);
    ui->loadBtn->click();
}

void MainWindow::resetBtnClicked()
{
    if(image==0||image->isNull())
        return;

    delete image;
    image=new QImage((uchar*)originalImageData,originalImageWidth,originalImageHeight,QImage::Format_ARGB32);
    scene->setSceneRect(0,0,originalImageWidth,originalImageHeight);
    pixmapItem->setPixmap(QPixmap::fromImage(*image));
    ui->graphicsView->viewport()->update();
    fitToWindow();
}

void MainWindow::pixelateBtnClicked()
{
    if(image==0||image->isNull())
        return;

    uint32_t *newImageData=(uint32_t*)malloc(originalImageWidth*originalImageHeight*sizeof(uint32_t));

    int cellSize=ui->cellSizeBox->value();
    int verCellCount=ceil(doubleDiv(originalImageHeight,cellSize));
    int horCellCount=ceil(doubleDiv(originalImageWidth,cellSize));
    const int originalImageHeightM1=originalImageHeight-1;
    const int originalImageWidthM1=originalImageWidth-1;

    for(int verCell=0;verCell<verCellCount;verCell++)
    {
        int verOffset=verCell*cellSize;
        const int yLimit=__min(verOffset+cellSize,originalImageHeightM1);
        for(int horCell=0;horCell<horCellCount;horCell++)
        {
            int horOffset=horCell*cellSize;
            const int xLimit=__min(horOffset+cellSize,originalImageWidthM1);
            double alphaSum=0.0;
            double redSum=0.0;
            double greenSum=0.0;
            double blueSum=0.0;

            // Sum values of source pixels

            for(int y=verOffset;y<=yLimit;y++)
            {
                int offset=y*originalImageWidth;
                for(int x=horOffset;x<=xLimit;x++)
                {
                    uint32_t color=originalImageData[offset+x];
                    alphaSum+=(double)getAlpha(color);
                    redSum+=(double)getRed(color);
                    greenSum+=(double)getGreen(color);
                    blueSum+=(double)getBlue(color);
                }
            }

            // Calculate average values

            double pixelCountInCell=(double)((yLimit-verOffset+1)*(xLimit-horOffset+1));
            uint32_t alphaAvg=round(alphaSum/pixelCountInCell);
            uint32_t redAvg=round(redSum/pixelCountInCell);
            uint32_t greenAvg=round(greenSum/pixelCountInCell);
            uint32_t blueAvg=round(blueSum/pixelCountInCell);

            uint32_t newColor=getColor(alphaAvg,redAvg,greenAvg,blueAvg);

            // Set pixels in destination

            for(int y=verOffset;y<=yLimit;y++)
            {
                int offset=y*originalImageWidth;
                for(int x=horOffset;x<=xLimit;x++)
                    newImageData[offset+x]=newColor;
            }
        }
    }

    delete image;
    image=new QImage((uchar*)newImageData,originalImageWidth,originalImageHeight,QImage::Format_ARGB32);
    pixmapItem->setPixmap(QPixmap::fromImage(*image));
    scene->setSceneRect(0,0,originalImageWidth,originalImageHeight);
    ui->graphicsView->viewport()->update();
    fitToWindow();
}

uint32_t *MainWindow::qImageToBitmapData(QImage *image)
{
    int32_t width=image->width();
    int32_t height=image->height();
    uint32_t *out=(uint32_t*)malloc(width*height*sizeof(uint32_t));
    for(int32_t y=0;y<height;y++)
    {
        int32_t offset=y*width;
        QRgb *scanLine=(QRgb*)image->scanLine(y); // Do not free!
        for(int32_t x=0;x<width;x++)
        {
            QRgb color=scanLine[x];
            uint32_t alpha=qAlpha(color);
            uint32_t red=qRed(color);
            uint32_t green=qGreen(color);
            uint32_t blue=qBlue(color);
            out[offset+x]=(alpha<<24)|(red<<16)|(green<<8)|blue;
        }
        // Do not free "scanLine"!
    }
    return out;
}
