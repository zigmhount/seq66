/*
 *  This file is part of seq66.
 *
 *  seq66 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  seq66 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with seq66; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file          qclocklayout.hpp
 *
 *  This class supports a MIDI Clocks label and a set of radio-buttons for
 *  selecting the clock style (off, on POS, on MOD), associating it with a
 *  particular output buss.
 *
 * \library       seq66 application
 * \author        Chris Ahlstrom
 * \date          2018-05-19
 * \updates       2020-12-16
 * \license       GNU GPLv2 or above
 *
 */

#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>

#include "play/performer.hpp"
#include "qclocklayout.hpp"

/*
 *  Do not document the namespace, it breaks Doxygen.
 */

namespace seq66
{
    class performer;

/**
 *  Creates a single line in the MIDI Clocks "Clock" group-box.  We will use
 *  the words "clock" or "port" for the MIDI output port represented by this
 *  widget.  Here are the jobs we have to do:
 *
 *      -#    Get the label for the port and set it.
 *      -#  Add the tooltips for the clock radio-buttons.
 *      -#  Add the clock radio-buttons to m_horizlayout_clocklive.
 *        -#    Connect to the radio-button slots:
 *            -    clock_callback_disable().
 *            -    clock_callback_off().
 *            -    clock_callback_on().
 *            -    clock_callback_mod().
 */

qclocklayout::qclocklayout
(
    QWidget * parent,
    performer & p,
    int bus
) :
    QWidget                     (parent),
    m_performance               (p),
    m_bus                       (bus),
    m_parent_widget             (parent),           /* m_groupbox_clocks */
    m_horizlayout_clockline     (nullptr),
    m_spacer_clock              (nullptr),
    m_label_outputbusname       (nullptr),
    m_rbutton_portdisabled      (nullptr),
    m_rbutton_clockoff          (nullptr),
    m_rbutton_clockonpos        (nullptr),
    m_rbutton_clockonmod        (nullptr),
    m_rbutton_group             (nullptr)
{
    setup_ui();

    bool ok = connect
    (
        m_rbutton_group, SIGNAL(buttonClicked(int)),
        this, SLOT(clock_callback_clicked(int))
    );
    if (! ok)
        (void) error_message("qclocklayout: clock-group slot failed to connect");
}

/**
 *  The tool-tips.
 *
 *  "This setting disables the usage of this output port, completely.  "
 *  "It is needed in some cases for devices that are detected, but "
 *  "cannot be used (e.g. devices locked by another application)."
 *
 *  "MIDI Clock will be disabled. Used for conventional playback."
 *
 *  "MIDI Clock will be sent. MIDI Song Position and MIDI Continue "
 *  "will be sent if starting after tick 0 in song mode; otherwise "
 *  "MIDI Start is sent."
 *
 *  "MIDI Clock will be sent.  MIDI Start will be sent and clocking "
 *  "will begin once the song position has reached the modulo of "
 *  "the specified Size. Use for gear that doesn't respond to Song "
 *  "Position."
 */

void
qclocklayout::setup_ui ()
{
    m_horizlayout_clockline = new QHBoxLayout();
    m_horizlayout_clockline->setContentsMargins(0, 0, 0, 0);
    m_spacer_clock = new QSpacerItem
    (
        20, 20, QSizePolicy::MinimumExpanding, QSizePolicy::Minimum
    );
    const clockslist & opm = output_port_map();
    mastermidibus * masterbus = perf().master_bus();
    if (opm.not_empty())
    {
        std::string busname = opm.get_name(bussbyte(m_bus));
        m_label_outputbusname = new QLabel();
        m_label_outputbusname->setText(busname.c_str());
    }
    else
    {
        if (not_nullptr(masterbus))
        {
            QString busname = masterbus->get_midi_out_bus_name(m_bus).c_str();
            m_label_outputbusname = new QLabel();
            m_label_outputbusname->setText(busname);
        }
        else
            return;
    }

    m_rbutton_portdisabled = new QRadioButton("Disabled");
    m_rbutton_clockoff = new QRadioButton("Off");
    m_rbutton_clockonpos = new QRadioButton("On(Pos)");
    m_rbutton_clockonmod = new QRadioButton("On(Mod)");

    m_rbutton_group = new QButtonGroup(this);
    m_rbutton_group->addButton(m_rbutton_portdisabled, int(e_clock::disabled));
    m_rbutton_group->addButton(m_rbutton_clockoff, int(e_clock::off));
    m_rbutton_group->addButton(m_rbutton_clockonpos, int(e_clock::pos));
    m_rbutton_group->addButton(m_rbutton_clockonmod, int(e_clock::mod));

    m_horizlayout_clockline->addWidget(m_label_outputbusname);
    m_horizlayout_clockline->addItem(m_spacer_clock);
    m_horizlayout_clockline->addWidget(m_rbutton_portdisabled);
    m_horizlayout_clockline->addWidget(m_rbutton_clockoff);
    m_horizlayout_clockline->addWidget(m_rbutton_clockonpos);
    m_horizlayout_clockline->addWidget(m_rbutton_clockonmod);

    e_clock clocking = opm.not_empty() ?
        clocking = opm.get(m_bus) : masterbus->get_clock(m_bus) ;

    switch (masterbus->get_clock(m_bus))
    {
    case e_clock::disabled:
        m_rbutton_portdisabled->setChecked(true);
        m_rbutton_portdisabled->setEnabled(false);
        m_rbutton_clockoff->setEnabled(false);
        m_rbutton_clockonpos->setEnabled(false);
        m_rbutton_clockonmod->setEnabled(false);
        break;

    case e_clock::off:
        m_rbutton_clockoff->setChecked(true);
        break;

    case e_clock::pos:
        m_rbutton_clockonpos->setChecked(true);
        break;

    case e_clock::mod:
        m_rbutton_clockonmod->setChecked(true);
        break;
    }
}

/**
 *  Sets the clocking value based on in incoming parameter.  We have to use
 *  this particular slot in order to handle all of the radio-buttons.
 *
 * \param id
 *      Provides the ID code of the button that was clicked.  We set these ID
 *      values explicitly, via addButton(ptrbutton, int(e_clock::disabled)).
 *      For some reason, probably because -1 is a special flag for this
 *      callback, -1 [e_clock::disabled] gets converted to -2.  So we have to
 *      adjust.
 */

void
qclocklayout::clock_callback_clicked (int id)
{
    if (id == (-2))
        id = (-1);

    perf().set_clock_bus(m_bus, static_cast<e_clock>(id));
    if (id == (-1))
    {
        m_rbutton_portdisabled->setEnabled(false);
        m_rbutton_clockoff->setEnabled(false);
        m_rbutton_clockonpos->setEnabled(false);
        m_rbutton_clockonmod->setEnabled(false);
    }
}

}           // namespace seq66

/*
 * qclocklayout.hpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

