#include "RenderLab.h"

#include <CppUtil/Qt/PaintImgOpCreator.h>
#include <CppUtil/Qt/RasterOpCreator.h>
#include <CppUtil/Qt/RawAPI_Define.h>
#include <CppUtil/Qt/OpThread.h>

#include <CppUtil/RTX/SceneCreator.h>
#include <CppUtil/RTX/RTX_Renderer.h>

#include <CppUtil/Basic/Image.h>

#include <CppUtil/Basic/LambdaOp.h>

#include <qdebug.h>
#include <qtimer.h>

#include <thread>

using namespace CppUtil::Basic;
using namespace RTX;
using namespace CppUtil::Qt;

RenderLab::RenderLab(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	// hide titlebar of dock_Top
	QWidget* lTitleBar = ui.dock_Top->titleBarWidget();
	QWidget* lEmptyWidget = new QWidget();
	ui.dock_Top->setTitleBarWidget(lEmptyWidget);
	delete lTitleBar;

	// update per frame
	QTimer * timer = new QTimer;
	timer->callOnTimeout([this]() {
		ui.OGLW_Raster->update();
		ui.OGLW_RayTracer->update();
	});

	const size_t fps = 60;
	timer->start(1000 / fps);

	// raster
	ui.OGLW_Raster->setFocusPolicy(Qt::ClickFocus);
	RasterOpCreator roc(ui.OGLW_Raster);
	auto rasterSceneOp = roc.GenScenePaintOp(1);
	rasterSceneOp->SetOp();

	// raytracer
	ui.OGLW_RayTracer->setFocusPolicy(Qt::ClickFocus);
	PaintImgOpCreator pioc(ui.OGLW_RayTracer);
	paintImgOp = pioc.GenScenePaintOp();
}

void RenderLab::on_btn_RenderStart_clicked(){
	ui.btn_RenderStart->setEnabled(false);
	ui.btn_RenderStop->setEnabled(true);

	OpThread::Ptr drawImgThread = ToPtr(new OpThread);
	drawImgThread->UIConnect(this, &RenderLab::UI_Op);

	auto drawImgOp = ToPtr(new LambdaOp([this, drawImgThread]() {
		paintImgOp->SetOp(1024, 768);
		auto img = paintImgOp->GetImg();
		auto scene = SceneCreator::Gen(RTX::SceneCreator::LOTS_OF_BALLS, 1024, 768);
		RTX_Renderer rtxRenderer(scene, img);

		OpThread::Ptr controller = ToPtr(new OpThread);
		controller->UIConnect(this, &RenderLab::UI_Op);
		auto controllOp = ToPtr(new LambdaOp([this, drawImgThread, controller, &rtxRenderer]() {
			while (!drawImgThread->IsStop()) {
				controller->UI_Op_Run([&, this]() {
					ui.rtxProgress->setValue(rtxRenderer.ProgressRate() * ui.rtxProgress->maximum());
				});
				Sleep(100);
			}
			rtxRenderer.Stop();
		}));
		controller->SetOp(controllOp);
		controller->start();

		rtxRenderer.Run();
		drawImgThread->Stop();
		controller->terminate();
		drawImgThread->UI_Op_Run([this]() {
			ui.btn_RenderStart->setEnabled(true);
			ui.btn_RenderStop->setEnabled(false);
			ui.rtxProgress->setValue(ui.rtxProgress->maximum());
		});
		qDebug() << "rtx is finished or stop\n";
	}));
	drawImgThread->SetOp(drawImgOp);

	GS::Reg("drawImgThread", drawImgThread);
	drawImgThread->start();
}

void RenderLab::on_btn_RenderStop_clicked() {
	ui.btn_RenderStart->setEnabled(true);
	ui.btn_RenderStop->setEnabled(false);
	OpThread::Ptr drawImgThread;
	GS::GetV("drawImgThread", drawImgThread);
	if (!drawImgThread)
		return;
	drawImgThread->Stop();
}

void RenderLab::UI_Op(Operation::Ptr op) {
	op->Run();
}