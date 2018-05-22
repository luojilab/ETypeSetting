#include "PreviewHTMLWindow.h"
#include <QPainter>
#include <QMouseEvent>
#include <QReadWriteLock>
#include <QFile>
#include <QDir>
#include <QSettings>

PreviewHTMLWindow::PreviewHTMLWindow(QWidget * parent, const std::string htmlPath)
	:QDockWidget(parent),
	m_htmlPath(htmlPath),
	m_engine(new LayoutEngine), 
	m_htmlPageIndex(0)
{
	m_engine->delegate = this;
	m_engine->SetViewTopMargin(0);
	m_engine->SetViewBottomMargin(0);
	m_innerHtmlPath = tempFilePath(htmlPath);
	QFile f(htmlPath.c_str());
	if (f.exists()) {
		f.copy(m_innerHtmlPath.c_str());
	}
	// Must initial engine at last!
	// in case the width and height value get wrong
	m_engine->InitLayoutEngine("");
}

PreviewHTMLWindow::~PreviewHTMLWindow()
{
	cleanResource();
	delete m_engine;
	delete &m_htmlPath;
	delete &m_innerHtmlPath;
}

void PreviewHTMLWindow::engineInitFinish() {
	int w = width();
	int h = height();
	m_engine->setPageSize(NULL, width(), height(), 1);
	m_engine->openHtml(this, m_innerHtmlPath, "html_id_key");
}

// engine epub delegate function
void PreviewHTMLWindow::engineOpenBook(BookModel* bookModel, QList<BookContents *>list, LAYOUT_ENGINE_OPEN_EPUB_STATUS error) {}
void PreviewHTMLWindow::engineClickResponse(const qint32& originX, const qint32& originY, const QString& chapterId, const qint32& htmlOffset) {}
void PreviewHTMLWindow::engineUpdateTotalCount(const qint32& totolPageCount) {}
void PreviewHTMLWindow::enginUpdateAllViewPage() {}
void PreviewHTMLWindow::engineUpdateVisibleViewPage(EngineHighlightInfo* info){}
void PreviewHTMLWindow::engineShowSelectionMenu(const qint32& x, const qint32& y, const qint32& x2, const qint32& y2){}
void PreviewHTMLWindow::engineOpenUrl(QString urlString){}
void PreviewHTMLWindow::engineClickImage(QImage* image, const qint32& originX, const qint32& originY, const qint32& width, const qint32& height){}
QList<BookUnderlineData *> PreviewHTMLWindow::engineNeedUnderlineData(const QString& charpterId) { return QList<BookUnderlineData *>(); }
QStringList PreviewHTMLWindow::engineNeedNoteData(const QString& charpterId) { return QStringList(); }
void PreviewHTMLWindow::enginePaintHighlightRect(const QRect& rect, const QColor& color) {}

/*
 * engine html delegate function 
 */

// html open success 
void PreviewHTMLWindow::engineOpenHTML(BookChapter *html, LAYOUT_ENGINE_OPEN_EPUB_STATUS error) {
	if (error == OPEN_EPUB_SUCCESS) {
		if (getHtmlModel() != NULL) {
			cleanResource();
		}
		setHtmlModel(html);
		m_engine->paintHtml(getHtmlModel(), m_htmlPageIndex);
	}
}
// html pic render finish
void PreviewHTMLWindow::htmlImageRenderFinish(BookChapter *html, std::shared_ptr<QImage>& pic) {
	safeSetRenderStatus(false);
	if (pic != nullptr) {
		m_pic = pic;
		m_old_htmlPageIndex = m_htmlPageIndex;
		update();
	} else {
		m_htmlPageIndex = m_old_htmlPageIndex;
	}
}

/*
 * system function
 */

// paint event
void PreviewHTMLWindow::paintEvent(QPaintEvent *) {
	if ( m_pic != nullptr && !(*m_pic).isNull() ) {
		QPainter p(this);
		if (p.isActive()) {
			p.eraseRect(QRect(0, 0, width(), height()));
			p.drawImage(QRect(0, 0, width(), height()), *m_pic);
			p.end();
		}
	}
}
// mouse click event
void PreviewHTMLWindow::mousePressEvent(QMouseEvent *event) {
	if (isRendering()) return;
	if (event->button() == Qt::LeftButton) {
		if (m_htmlPageIndex + 1 >= getHtmlModel()->GetPageCount()) return;
		++m_htmlPageIndex;
	}
	else if (event->button() == Qt::RightButton) {
		if (m_htmlPageIndex == 0) return;
		--m_htmlPageIndex;
	}
	m_engine->paintHtml(getHtmlModel(), m_htmlPageIndex);
}
// close event
void PreviewHTMLWindow::closeEvent(QCloseEvent *) {
	cleanResource();
	delete m_htmlModel;
	m_pic.reset();
}

void PreviewHTMLWindow::reloadHTML(std::string htmlPath, bool reload)
{
	if (getHtmlModel() || reload) {
		cleanResource();
		m_htmlPath = htmlPath;
		QFile f(htmlPath.c_str());
		m_htmlPath = htmlPath;
		m_innerHtmlPath = tempFilePath(m_htmlPath);
		if ( !QFile(tempFilePath(htmlPath).c_str()).exists() ) {
		f.copy(tempFilePath(m_htmlPath).c_str());
		}
		m_htmlPageIndex = 0;
		m_engine->setPageSize(NULL, width(), height(), 1);
		m_engine->openHtml(this, m_htmlPath, "html_id_key");
	}
}

void PreviewHTMLWindow::updateCurrentPage(const QString& contentTexts)
{
	//if (getHtmlModel()) {
		cleanResource();
		QFile f(m_innerHtmlPath.c_str());
		if (f.open(QIODevice::ReadWrite | QIODevice::Text)) {
		f.write(contentTexts.toUtf8());
		f.close();
		}
		m_engine->openHtml(this, m_innerHtmlPath, "html_id_key");
	//}
}

void PreviewHTMLWindow::cleanTempFile()
{
	QDir dir(QFileInfo(m_htmlPath.c_str()).absolutePath());
	QFileInfoList file_list = dir.entryInfoList(QDir::Files | QDir::NoSymLinks);
	for each (QFileInfo f_info in file_list)
	{
		if (f_info.fileName().startsWith("__Temp__")) 
		{
			QFile(f_info.filePath()).remove();
		}
	}
}

void PreviewHTMLWindow::safeSetRenderStatus(bool status)
{
	m_locker.lockForRead();
	m_isRendering = status;
	m_locker.unlock();
}

inline bool PreviewHTMLWindow::safeGetRenderStatus()
{
	m_locker.lockForRead();
	bool status = m_isRendering;
	m_locker.unlock();
	return status;
}

inline void PreviewHTMLWindow::setHtmlModel(BookChapter *html)
{
	m_ModelLocker.lockForWrite();
	m_htmlModel = html;
	m_ModelLocker.unlock();
}

inline BookChapter * PreviewHTMLWindow::getHtmlModel()
{
	BookChapter *model;
	m_ModelLocker.lockForRead();
	model = m_htmlModel;
	m_ModelLocker.unlock();
	return model;
}

void PreviewHTMLWindow::cleanResource()
{
	if (m_pic) {
		m_pic.reset();
	}
	if (getHtmlModel()) {
		m_engine->closeHtml(getHtmlModel());
		setHtmlModel(NULL);
	}
}

std::string PreviewHTMLWindow::tempFilePath(const std::string & ofilePath)
{
	if (ofilePath.empty()) {
		return "";
	}

	QFileInfo oinfo(ofilePath.c_str());
	std::string fileName = "__Temp__" + oinfo.fileName().toStdString();
	std::string dir = oinfo.absolutePath().toStdString() + "/";
	if (fileName.empty()) {
		return "";
	}
	return dir + fileName;
}