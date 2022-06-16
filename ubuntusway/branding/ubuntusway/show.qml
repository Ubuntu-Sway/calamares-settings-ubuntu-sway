/* === This file is part of Calamares - <http://github.com/calamares> ===
 *
 *   Copyright 2015, Teo Mrnjavac <teo@kde.org>
 *   Copyright 2018-2019, Jonathan Carter <jcc@debian.org>
 *
 *   Calamares is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, or (at your option) any later version.
 *
 *   Calamares is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Calamares. If not, see <http://www.gnu.org/licenses/>.
 */

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
            source: "TUI.png"
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
            source: "Multimedia.png"
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
            source: "Internet.png"
        }
    }
    Slide {
        Image {
            anchors.centerIn: parent
            id: image6
            x:0
            y:0
            width: 810
            height: 485
            fillMode: Image.PreserveAspectFit
            smooth: true
            source: "Productive.png"
        }
    }
    Slide {
        Image {
            anchors.centerIn: parent
            id: image7
            x:0
            y:0
            width: 810
            height: 485
            fillMode: Image.PreserveAspectFit
            smooth: true
            source: "Accessories.png"
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
