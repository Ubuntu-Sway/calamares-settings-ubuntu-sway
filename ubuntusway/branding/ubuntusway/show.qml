import QtQuick 2.0;
import calamares.slideshow 1.0;

Presentation
{
    id: presentation

    Timer {
        interval: 20000
        running: true
        repeat: true
        onTriggered: presentation.goToNextSlide()
    }
    Slide {
        Image {
            anchors.centerIn: parent
            id: image1
            x:0
            y:0
            width: 810
            height: 485
            fillMode: Image.PreserveAspectFit
            smooth: true
            source: "Features.png"
        }
    }
    Slide {
        Image {
            anchors.centerIn: parent
            id: image3
            x:0
            y:0
            width: 810
            height: 485
            fillMode: Image.PreserveAspectFit
            smooth: true
            source: "TUI.png"
        }
    }
    Slide {
        Image {
            anchors.centerIn: parent
            id: image4
            x:0
            y:0
            width: 810
            height: 485
            fillMode: Image.PreserveAspectFit
            smooth: true
            source: "Customize.png"
        }
    }
    Slide {
        Image {
            anchors.centerIn: parent
            id: image5
            x:0
            y:0
            width: 810
            height: 485
            fillMode: Image.PreserveAspectFit
            smooth: true
            source: "Hardware.png"
        }
    }
    Slide {
        Image {
            anchors.centerIn: parent
            id: image2
            x:0
            y:0
            width: 810
            height: 485
            fillMode: Image.PreserveAspectFit
            smooth: true
            source: "FindUs.png"
        }
    }
}
