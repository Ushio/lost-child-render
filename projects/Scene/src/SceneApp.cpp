#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class SceneApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
};

void SceneApp::setup()
{
}

void SceneApp::mouseDown( MouseEvent event )
{
}

void SceneApp::update()
{
}

void SceneApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) ); 
}

CINDER_APP( SceneApp, RendererGl )
