/*
 *  This file is part of seq66.
 *
 *  seq66 is free software; you can redistribute it and/or modify it under the
 *  terms of the GNU General Public License as published by the Free Software
 *  Foundation; either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  seq66 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with seq66; if not, write to the Free Software Foundation, Inc., 59 Temple
 *  Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * \file          qseqroll.cpp
 *
 *  This module declares/defines the base class for drawing on the piano
 *  roll of the patterns editor for the Qt 5 implementation.
 *
 * \library       seq66 application
 * \author        Chris Ahlstrom
 * \date          2018-01-01
 * \updates       2023-06-24
 * \license       GNU GPLv2 or above
 *
 *  Please see the additional notes for the Gtkmm-2.4 version of this panel,
 *  seqroll.
 */

#include <QApplication>                 /* QApplication keyboardModifiers() */
#include <QFrame>                       /* base class for seqedit frame(s)  */
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QTimer>

#include "cfg/settings.hpp"             /* seq66::usr().key_height(), etc.  */
#include "play/performer.hpp"           /* seq66::performer class           */
#include "qseqeditframe64.hpp"          /* seq66::qseqeditframe64 class     */
#include "qseqkeys.hpp"                 /* seq66::qseqkeys class            */
#include "qseqroll.hpp"                 /* seq66::qseqroll class            */
#include "qt5_helpers.hpp"              /* seq66::qt() string conversion    */

/**
 *  We've had an issue where adding wrapped-but-truncated notes would
 *  alter the look of some other notes until the play/record was stopped.
 *  So this macro enables sequence::verify_and_link() every time.
 *  This iterates through all events, but acts only for unlinked notes,
 *  so it doesn't appear add a noticeable amount to the CPU load.
 */

#define SEQ66_ALWAYS_VERIFY_AND_LINK

/*
 *  Do not document a namespace; it breaks Doxygen.
 */

namespace seq66
{

/**
 *  Default value for randomization.  Currently the only value supported.
 */

static const int c_randomize_range  = 4;        /* randomize range in ticks */
static const int c_border_width     = 2;
static const int c_pen_width        = 1;

/**
 *  Principal constructor.
 */

qseqroll::qseqroll
(
    performer & p,
    sequence & s,
    qseqeditframe64 * frame,
    qseqkeys * seqkeys_wid,
    int zoom, int snap,
    sequence::editmode mode,
    int unith, int totalh
) :
    QWidget                 (frame),
    qseqbase                (p, s, frame, zoom, snap, unith, totalh),
    m_analysis_msg          (nullptr),
    m_font                  ("Monospace"),
    m_backseq_color         (backseq_paint()),
    m_seqkeys_wid           (seqkeys_wid),
    m_timer                 (nullptr),
    m_progbar_width         (usr().progress_bar_thick() ? 2 : 1),
    m_scale                 (scales::off),
    m_pos                   (0),
    m_chord                 (0),
    m_key                   (0),
    m_note_length           (p.ppqn() * 4 / 16),
    m_note_off_margin       (2),
    m_background_sequence   (seq::unassigned()),
    m_draw_background_seq   (false),
    m_status                (0),
    m_cc                    (0),
    m_edit_mode             (mode),
    m_draw_whole_grid       (true),
    m_t0                    (0),
    m_t1                    (0),
    m_frame_ticks           (0),
    m_note_x                (0),
    m_note_width            (0),
    m_note_y                (0),
    m_keypadding_x          (c_keyboard_padding_x),
    m_v_zooming             (false),
    m_last_base_note        (-1),
    m_link_wraparound       (usr().new_pattern_wraparound())
{
    setAttribute(Qt::WA_StaticContents);
    setAttribute(Qt::WA_OpaquePaintEvent);          /* no erase on repaint  */
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);                         /* track w/out a click  */
    m_font.setStyleHint(QFont::Monospace);
    m_font.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    m_font.setBold(false);
    m_font.setPointSize(6);                         /* 8 is too obtrusive   */
    set_snap(track().snap());
    show();
    m_timer = qt_timer(this, "qseqroll", 1, SLOT(conditional_update()));
}

/**
 *  This virtual destructor stops the timer.
 */

qseqroll::~qseqroll ()
{
    m_timer->stop();
}

/**
 *  In an effort to reduce CPU usage when simply idling, this function calls
 *  update() only if necessary.  See qseqbase::check_dirty().
 *
 *  bool ok = track().playing();
 */

void
qseqroll::conditional_update ()
{
    bool ok = perf().needs_update() || check_dirty();
    if (ok)
    {
#if defined SEQ66_ALWAYS_VERIFY_AND_LINK
        if (track().recording())
            track().verify_and_link();      /* refresh before update        */
#endif
        if (progress_follow())
            follow_progress();              /* keep up with progress        */

        update();
    }
}

void
qseqroll::set_dirty ()
{
    /*
     * Recursion unto segfault!
     *
     *      frame64()->set_track_change();
     *
     * This function is actually for drawing.
     *
     *      frame64()->set_external_frame_title();
     */

    qseqbase::set_dirty();
}

/**
 *  Zooms in, first calling the base-class version of this function, then
 *  passing along the message to the parent edit frame, so that it can change
 *  the zoom on the other panels of the parent edit frame.
 */

bool
qseqroll::zoom_in ()
{
    bool result = qseqbase::zoom_in();
    if (result)
    {
        result = frame64()->set_zoom(zoom());
        set_dirty();
    }
    return result;
}

/**
 *  Zooms out, first calling the base-class version of this function, then
 *  passing along the message to the parent edit frame, so that it can change
 *  the zoom on the other panels of the parent edit frame.
 */

bool
qseqroll::zoom_out ()
{
    bool result = qseqbase::zoom_out();
    if (result)
    {
        result = frame64()->set_zoom(zoom());
        set_dirty();
    }
    return result;
}

/**
 *  Tells the parent frame to reset our zoom.
 */

bool
qseqroll::reset_zoom ()
{
    bool result = frame64()->reset_zoom();
    set_dirty();
    return result;
}

bool
qseqroll::v_zoom_in ()
{
    bool result = m_seqkeys_wid->v_zoom_in();
    if (result)
    {
        int h = m_seqkeys_wid->note_height();
        unit_height(h);
        total_height(m_seqkeys_wid->total_height());
        m_v_zooming = true;
        set_dirty();
        frame64()->set_dirty();
    }
    return result;
}

bool
qseqroll::v_zoom_out ()
{
    bool result = m_seqkeys_wid->v_zoom_out();
    if (result)
    {
        int h = m_seqkeys_wid->note_height();
        unit_height(h);
        total_height(m_seqkeys_wid->total_height());
        m_v_zooming = true;
        set_dirty();
        frame64()->set_dirty();
    }
    return result;
}

bool
qseqroll::reset_v_zoom ()
{
    bool result = m_seqkeys_wid->reset_v_zoom();
    if (result)
    {
        int h = m_seqkeys_wid->note_height();
        unit_height(h);
        total_height(m_seqkeys_wid->total_height());
        set_dirty();
        frame64()->set_dirty();
    }
    m_v_zooming = false;
    return result;
}

int
qseqroll::note_height () const
{
    return m_seqkeys_wid->note_height();
}

/**
 *  Override.
 *
 * \param v
 *      The value of the scrollbar in pixels.
 */

void
qseqroll::scroll_offset (int x)
{
    midipulse ticks = pix_to_tix(x);
    midipulse ticks_per_step = pulses_per_substep(perf().ppqn(), zoom());
    m_t0 = ticks - (ticks % ticks_per_step);
    m_frame_ticks = pix_to_tix(frame64()->width());
    m_t1 = ticks + m_frame_ticks;
    qseqbase::scroll_offset(x);
}

/**
 *  This function sets the given sequence onto the piano roll of the pattern
 *  editor, so that the musician can have another pattern to play against.
 *  The state parameter sets the boolean m_draw_background_seq.
 *
 *  The first check in this statement disabled further changes!
 *
 *      if (state != m_draw_background_seq && m_background_sequence != seq)
 *
 * \param state
 *      If true, the background sequence will be drawn.
 *
 * \param seq
 *      Provides the sequence number, which is checked against seq::legal()
 *      before being used.  This macro allows the value seq::limit (2048),
 *      which disables the background sequence.
 */

void
qseqroll::set_background_sequence (bool state, int seq)
{
    if (m_background_sequence != seq)
    {
        if (seq::legal(seq))
        {
            m_draw_background_seq = state;
            m_background_sequence = seq;
        }
        if (is_initialized())
            set_dirty();
    }
}

/**
 *  Does anybody use this one? qseqeditframe64::on_automation_change().
 */

void
qseqroll::set_redraw ()
{
    m_draw_whole_grid = true;
    set_dirty();
}

/**
 *  Draws the piano roll.
 *
 *  In later usage, the width() function [and height() as well?], returns a
 *  humongous value (38800+).  So we store the current values to use, via
 *  window_width() and window_height(), in follow_progress().
 *
 *  We have to decide how to handle repaints after the initialization.  Do we
 *  use the QRect of the paint-event?
 *
 *  Here, we could choose black instead white for "inverse" mode.
 */

void
qseqroll::paintEvent (QPaintEvent * qpep)
{
    QRect r = qpep->rect();
    QRect view(0, 0, width(), height());
    QPainter painter(this);
    QBrush brush(blank_brush());    // QBrush brush(Qt::white, Qt::NoBrush);
    QPen pen(Qt::lightGray);
    pen.setStyle(Qt::SolidLine);
    pen.setColor(Qt::lightGray);
    painter.setPen(pen);
    painter.setFont(m_font);
    m_frame_ticks = pix_to_tix(r.width());
    m_edit_mode = perf().edit_mode(track().seq_number());

    /*
     * Draw the border and grid. See the banner notes about width and height.
     * Doesn't seem to be needed: painter.drawRect(0, 0, ww, wh);
     */

    draw_grid(painter, view);
    set_initialized();

    /*
     * Draw the events. This currently draws all of them.  Drawing all them
     * only needs to be drawn once.
     */

    call_draw_notes(painter, view);
    pen.setWidth(c_pen_width);

    /*
     *  Draw the playhead.
     */

    pen.setColor(progress_color());
    pen.setStyle(Qt::SolidLine);
    pen.setWidth(m_progbar_width);
    painter.setPen(pen);
    old_progress_x(progress_x());
    progress_x(xoffset(track().get_tick()));
    painter.drawLine(progress_x(), r.y(), progress_x(), r.y() + r.height());

    /*
     * End of draw_progress_on_window().  The next step is to restore
     * the "empty" brush style in case the user draws a selection box.
     */

    brush.setStyle(Qt::NoBrush);            /* painter reset                */
    painter.setBrush(brush);
    if (select_action())                    /* select/move/paste/grow       */
        pen.setStyle(Qt::SolidLine);

    int x, y, w, h;                         /* draw selections              */
    if (selecting())
    {
        rect::xy_to_rect_get
        (
            drop_x(), drop_y(), current_x(), current_y(), x, y, w, h
        );
        old_rect().set(x, y, w, h + unit_height());
        pen.setColor(sel_color());
        painter.setPen(pen);
        painter.drawRect(x, y, w, h);
    }
    else if (paste())                       /* issue #97, draw a paste box  */
    {
        pen.setColor(Qt::gray);
        painter.setPen(pen);
        painter.drawRect
        (
            current_x(), current_y(),
            old_rect().width(), old_rect().height()
        );
    }

    int selw = selection().width();
    int selh = selection().height();
    if (drop_action())
    {
        int delta_x = current_x() - drop_x();
        int delta_y = current_y() - drop_y();
        x = selection().x() + delta_x;
        y = selection().y() + delta_y;
        pen.setColor(Qt::black);            /* what palette color to use?   */
        painter.setPen(pen);
        if (is_drum_mode())
        {
            int drumx = x - unit_height() * 0.5 + m_keypadding_x;
            painter.drawRect(drumx, y, selw + unit_height(), selh);
        }
        else
            painter.drawRect(x + m_keypadding_x, y, selw, selh);

        old_rect().set(x, y, selw, selh);
    }
    if (growing())
    {
        int delta_x = current_x() - drop_x();
        selw += delta_x;
        if (selw < 1)
            selw = 1;

        x = selection().x();
        y = selection().y();
        pen.setColor(sel_color());          /* fore_color() Qt::black       */
        painter.setPen(pen);
        painter.drawRect(x + m_keypadding_x, y, selw, selh);
        old_rect().set(x, y, selw, selh);
    }
}

void
qseqroll::call_draw_notes (QPainter & painter, const QRect & view)
{
    if (m_draw_background_seq)
        draw_notes(painter, view, true);

    if (is_drum_mode())
        draw_drum_notes(painter, view, false);
    else
        draw_notes(painter, view, false);
}

/**
 *  First, we clear the rectangle before drawing.  At this point, we could
 *  choose black instead white for "inverse" mode.
 *
 *  The brush. The following really looks good only with the default
 *  "SolidPattern" style:
 *
 *          QBrush brush(back_color(), blank_brush().style());
 *
 *  Drawing the horizontal grid lines depends on the vertical zoom.
 *  Draw horizontal grid lines differently depending on editing mode.
 *  Set line color dependent on the note row we're on.
 *
 *  Drawing vertical grid lines.  Incrementing by ticks_per_step only works
 *  for PPQN of certain multiples or for certain time offsets.  Therefore, need
 *  to check every darn tick!!!! No, that causes aliasing in drawing horizontal
 *  lines. :-D
 *
 *          for (int tick = starttick; tick < endtick; ++tick)
 *
 *  The ticks_per_step value needs to be figured out.  Why 6 * zoom()?  6 is
 *  the number of pixels in the smallest divisions in the default seqroll
 *  background.  This code needs to be put into a function.
 *
 *  For odd beat widths, use 1 as ticks_per_substep.
 */

void
qseqroll::draw_grid (QPainter & painter, const QRect & r)
{
    int octkey = c_octave_size - m_key;             /* used three times     */
    QBrush brush(back_color());                     /* brush(Qt::NoBrush)   */
    QPen pen(grey_color());                         /* pen(Qt::lightGray)   */
    pen.setStyle(Qt::SolidLine);                    /* Qt::DotLine          */
    pen.setWidth(c_border_width);                   /* border thickness     */
    painter.fillRect(r, brush);                     /* blank the viewport   */
    painter.setBrush(brush);
    painter.setPen(pen);
    painter.drawRect(r);
    pen.setWidth(c_pen_width);                          /* line thickness   */

    /*
     * Horizontal (note) lines.
     */

    for (int key = 1; key <= c_notes_count; ++key)      /* each note row    */
    {
        int remkeys = c_notes_count - key;              /* remaining keys   */
        int modkey = remkeys - scroll_offset_v() + octkey;
        int y = key * unit_height() + 2;
        if ((modkey % c_octave_size) == 0)
            pen.setColor(fore_color());
        else
            pen.setColor(step_color());

        painter.setPen(pen);
        painter.drawLine(r.x(), y, r.x() + r.width(), y);
        if (m_scale != scales::off)
        {
            if (! scales_policy(m_scale, modkey))       /* scales.cpp/hpp   */
            {
                pen.setColor(fore_color());         /* Qt::lightGray        */
                painter.setBrush(scale_brush());
                painter.setPen(pen);
                painter.drawRect(0, y + 1, r.width(), unit_height() - 1);
            }
        }
    }

#if defined SEQ66_TIME_SIG_DRAWING

    int count = track().time_signature_count();
    for (int tscount = 0; tscount < count; ++tscount)
    {
        const sequence::timesig & ts = track().get_time_signature(tscount);
        if (ts.sig_beat_width == 0)
            break;

        int bpbar = ts.sig_beats_per_bar;
        int bwidth = ts.sig_beat_width;
        midipulse ticks_per_beat = (4 * perf().ppqn()) / bwidth;
        midipulse ticks_per_bar = bpbar * ticks_per_beat;
        midipulse ticks_per_step = pulses_per_substep(perf().ppqn(), zoom());
        midipulse starttick = ts.sig_start_tick;
        midipulse endtick = ts.sig_end_tick != 0 ?
            ts.sig_end_tick : pix_to_tix(r.x() + r.width());

        starttick -= starttick % ticks_per_step;
        if ((bwidth % 2) != 0)
            ticks_per_step = zoom();

        for (midipulse tick = starttick; tick < endtick; tick += ticks_per_step)
        {
            int x_offset = xoffset(tick) - scroll_offset_x();
            int penwidth = 1;
            enum Qt::PenStyle penstyle = Qt::SolidLine;

#if defined USE_DASHED_LINE_TO_DENOTE_TIME_SIG

            /*
             * This doesn't seem intuitive enough for a user.
             */

            if ((tscount % 2) == 1)
                penstyle = Qt::DashLine;            /* show time-sig change */
#endif

            if (tick % ticks_per_bar == 0)          /* solid line every bar */
            {
                pen.setColor(fore_color());         /* Qt::black            */
                penwidth = 2;
            }
            else if (tick % ticks_per_beat == 0)    /* light on every beat  */
            {
                pen.setColor(beat_color());
                penwidth = 1;
            }
            else
            {
                pen.setColor(step_color());         /* faint step lines     */
                int tick_snap = tick - (tick % grid_snap());
                if (tick != tick_snap)
                    penstyle = Qt::DotLine;
            }
            pen.setWidth(penwidth);
            pen.setStyle(penstyle);
            painter.setPen(pen);
            painter.drawLine(x_offset, 0, x_offset, total_height());
        }
    }

#else

    int bpbar = track().get_beats_per_bar();
    int bwidth = track().get_beat_width();
    midipulse ticks_per_beat = (4 * perf().ppqn()) / bwidth;
    midipulse ticks_per_bar = bpbar * ticks_per_beat;
    midipulse ticks_per_step = pulses_per_substep(perf().ppqn(), zoom());
    midipulse starttick = pix_to_tix(r.x());
    midipulse endtick = pix_to_tix(r.x() + r.width());
    starttick -= starttick % ticks_per_step;
    if ((bwidth % 2) != 0)
        ticks_per_step = zoom();                            /* EXPERIMENTAL */

    for (int tick = starttick; tick < endtick; tick += ticks_per_step)
    {
        int x_offset = xoffset(tick) - scroll_offset_x();
        int penwidth = 1;
        enum Qt::PenStyle penstyle = Qt::SolidLine;
        if (tick % ticks_per_bar == 0)          /* solid line on every bar  */
        {
            pen.setColor(fore_color());         /* Qt::black                */
            penwidth = 2;
        }
        else if (tick % ticks_per_beat == 0)    /* lighter on every beat    */
        {
            pen.setColor(beat_color());
            penwidth = 1;
        }
        else
        {
            pen.setColor(step_color());         /* faint step lines         */
            int tick_snap = tick - (tick % grid_snap());
            if (tick != tick_snap)
                penstyle = Qt::DotLine;
        }
        pen.setWidth(penwidth);
        pen.setStyle(penstyle);
        painter.setPen(pen);
        painter.drawLine(x_offset, 0, x_offset, total_height());
    }

#endif  // defined SEQ66_TIME_SIG_DRAWING
}

/**
 * Draw the current pixmap frame.  Note that, if the width and height change, we
 * will have to reevaluate.  Draw the events. This currently draws all of them.
 * Drawing all them only needs to be drawn once.
 */

void
qseqroll::draw_notes
(
    QPainter & painter,
    const QRect & r,
    bool background
)
{
    QBrush brush(note_brush());
    QBrush error_brush(Qt::magenta);    /* for unlinked notes               */
    QPen pen(fore_color());
    QPen error_pen(Qt::magenta);
    pen.setStyle(Qt::SolidLine);
    pen.setWidth(c_pen_width);
    painter.setPen(pen);
    painter.setBrush(brush);

    midipulse seqlength = track().get_length();
    midipulse start_tick = pix_to_tix(r.x());
    midipulse end_tick = start_tick + pix_to_tix(r.width());
    sequence * b = perf().get_sequence(m_background_sequence).get();
    sequence * s = background ? b : &track();
    if (is_nullptr(s))
        return;

    int unitheight = unit_height();
    int unitdecr = unit_height() - 2;
    int noteheight = unitheight - 3;
    s->draw_lock();
    for (auto cev = s->cbegin(); ! s->cend(cev); ++cev)
    {
        sequence::note_info ni;
        sequence::draw dt = s->get_next_note(ni, cev);
        if (dt == sequence::draw::finish)
            break;

        if (dt == sequence::draw::tempo)
        {
#if defined SEQ66_SHOW_TEMPO_IN_PIANO_ROLL
            int x = xoffset(ni.start());
            double tempo = double(ni.velocity());
            int tnote = tempo_to_note_value(tempo);
            int y = total_height() - (tnote * unitheight) - unitdecr;
            pen.setColor(fore_color());
            brush.setColor(tempo_color());
            painter.setPen(pen);
            painter.setBrush(brush);
            draw_tempo(painter, x, y, ni.velocity());
#endif
            continue;
        }

        bool start_in = ni.start() >= start_tick && ni.start() <= end_tick;
        bool end_in = ni.finish() >= start_tick && ni.finish() <= end_tick;
        bool not_wrapped = ni.finish() >= ni.start();
        bool linkedin = dt == sequence::draw::linked && end_in;
        bool bad = false;
        if (start_in || linkedin)
        {
            int in_shift = 0;
            int length_add = 0;
            m_note_x = xoffset(ni.start());
            m_note_y = total_height() - (ni.note() * unitheight) - unitdecr;
            if (dt == sequence::draw::linked)
            {
                if (not_wrapped)
                {
                    m_note_width = tix_to_pix(ni.finish() - ni.start());
                    if (m_note_width < 1)
                        m_note_width = 1;
                }
                else
                    m_note_width = tix_to_pix(seqlength - ni.start());
            }
            else
                m_note_width = tix_to_pix(16);

            if (dt == sequence::draw::note_on)      /* means it's unlinked  */
            {
                in_shift = 0;
                length_add = 2;
                bad = true;
                painter.setBrush(error_brush);
            }
            else if (dt == sequence::draw::note_off)
            {
                in_shift = -1;
                length_add = 1;
                bad = true;
                painter.setBrush(error_brush);
            }
            if (background)                         /* draw background note */
            {
                length_add = 1;
                painter.setBrush(backseq_brush());
            }
            else
            {
                painter.setBrush(note_brush());
            }
            painter.drawRect(m_note_x, m_note_y, m_note_width, noteheight);
            if (use_gradient())
            {
                if (background)
                {
                    length_add = 1;
                    painter.setBrush(backseq_brush());
                    painter.drawRect
                    (
                        m_note_x, m_note_y, m_note_width, noteheight
                    );
                }
                else
                {
                    QLinearGradient grad
                    (
                        m_note_x, m_note_y,
                        m_note_x, m_note_y + noteheight
                    );
                    grad.setColorAt(0.05, fore_color());
                    grad.setColorAt(0.5,  note_in_color());
                    grad.setColorAt(0.95, fore_color());
                    painter.fillRect
                    (
                        m_note_x + 1, m_note_y + 1, m_note_width - 1,
                        noteheight - 1, grad
                    );
                }
            }
            if (m_link_wraparound && ! not_wrapped)
            {
                int len = tix_to_pix(ni.finish()) - m_note_off_margin;
                if (use_gradient())
                {
                    QLinearGradient grad
                    (
                        m_keypadding_x, m_note_y,
                        m_keypadding_x, m_note_y + noteheight
                    );
                    grad.setColorAt(0.05, fore_color());
                    grad.setColorAt(0.5,  Qt::magenta);
                    grad.setColorAt(0.95, fore_color());
                    painter.fillRect
                    (
                        m_keypadding_x, m_note_y,
                        len + 1, noteheight + 1, grad
                    );
                }
                else
                {
                    painter.setPen(error_pen);
                    painter.drawRect
                    (
                        m_keypadding_x, m_note_y, len, noteheight
                    );
                    painter.setPen(pen);
                }
            }

            /*
             * Draw note highlight if there's room.  Orange note if selected,
             * red if drum mode, otherwise plain white.
             */

            if (m_note_width > 3)
            {
                if (! background)
                {
                    int x_shift = m_note_x + in_shift;
                    int h_minus = noteheight - 1;
                    if (use_gradient())
                    {
                        if (ni.selected())
                        {
                            QLinearGradient grad
                            (
                                x_shift, m_note_y, m_note_x, m_note_y + h_minus
                            );
                            grad.setColorAt(0.01, fore_color());
                            grad.setColorAt(0.5,  sel_color());
                            grad.setColorAt(0.99, fore_color());
                            painter.fillRect
                            (
                                x_shift, m_note_y,
                                m_note_width + length_add - 1, h_minus, grad
                            );
                        }
                    }
                    else
                    {
                        if (ni.selected())
                            brush.setColor(sel_color());        /* "orange"  */
                        else
                            brush.setColor(note_in_color());    /* Qt::white */

                        if (bad)
                            painter.setBrush(error_brush);
                        else
                            painter.setBrush(brush);

                        if (not_wrapped)                /* note highlight   */
                        {
                            painter.drawRect
                            (
                                x_shift, m_note_y,
                                m_note_width + length_add - 1, h_minus
                            );
                        }
                        else
                        {
                            int w = tix_to_pix(ni.finish()) + length_add - 3;
                            painter.drawRect
                            (
                                x_shift, m_note_y, m_note_width, h_minus
                            );
                            painter.drawRect
                            (
                                m_keypadding_x, m_note_y, w, h_minus
                            );
                        }
                    }
                }
            }
        }
    }
    s->draw_unlock();
}

/*
 * Why floating point; just divide by 2.  Also, the polygon seems to be offset
 * downard by half the note height.
 *
\verbatim
             x0    x     x1
        y          1
                   .
                  / \
                 /   \
        y0   0  .     . 2
                 \   /
                  \ /
        y1         .
                   3
\endverbatim
 */

void
qseqroll::draw_drum_note (QPainter & painter, int x, int y)
{
    int noteheight = unit_height();
    int h2 = noteheight / 2;
    int x0 = x - h2;
    int x1 = x + h2;
    int y0 = y + h2;
    int y1 = y + noteheight;
    QPointF points[4] =
    {
        QPointF(x0, y0),    // 0
        QPointF(x,  y),     // 1
        QPointF(x1, y0),    // 2
        QPointF(x,  y1)     // 3
    };
    painter.drawPolygon(points, 4);

    /*
     * Draw note highlight.  Not really useful, save time by ignoring.
     *
     * if (ni.selected())
     *     brush.setColor("orange");       // Qt::red
     * else if (is_drum_mode())
     *     brush.setColor(Qt::red);
     */
}

#if defined SEQ66_SHOW_TEMPO_IN_PIANO_ROLL

void
qseqroll::draw_tempo (QPainter & painter, int x, int y, int velocity)
{
    QString v = qt(std::to_string(velocity));
    int h = int(0.75 * unit_height());
    painter.drawEllipse(x, y, h, h);
    painter.drawText(x, y - 2, v);
}

#endif

void
qseqroll::draw_drum_notes
(
    QPainter & painter,
    const QRect & r,
    bool background
)
{
    QBrush brush(Qt::NoBrush);
    QPen pen(drum_color());         /* draw red boxes from drum loop    */
    pen.setStyle(Qt::SolidLine);
    pen.setWidth(c_pen_width);
    brush.setStyle(Qt::SolidPattern);
    painter.setPen(pen);
    painter.setBrush(brush);
    m_edit_mode = perf().edit_mode(track().seq_number());

    midipulse start_tick = pix_to_tix(r.x());
    midipulse end_tick = start_tick + pix_to_tix(r.width());
    sequence * b = perf().get_sequence(m_background_sequence).get();
    sequence * s = background ? b : &track();
    if (is_nullptr(s))
        return;

    int noteheight = unit_height();
    for (auto cev = s->cbegin(); ! s->cend(cev); ++cev)
    {
        sequence::note_info ni;
        sequence::draw dt = s->get_next_note(ni, cev);
        if (dt == sequence::draw::finish)
            break;

        if (dt == sequence::draw::tempo)
        {
#if defined SEQ66_SHOW_TEMPO_IN_PIANO_ROLL
            int x = xoffset(ni.start());
            midibpm max = usr().midi_bpm_maximum();
            midibpm min = usr().midi_bpm_minimum();
            double tempo = double(ni.velocity());
            int y = int((max - tempo) / (max - min) * 128) + 0;
            pen.setColor(fore_color());
            brush.setColor(tempo_color());
            painter.setPen(pen);
            painter.setBrush(brush);
            draw_tempo(painter, x, y, ni.velocity());
#endif
            continue;
        }

        bool start_in = ni.start() >= start_tick && ni.start() <= end_tick;
        bool end_in = ni.finish() >= start_tick && ni.finish() <= end_tick;
        bool linkedin = dt == sequence::draw::linked && end_in;
        if (start_in || linkedin)
        {
            m_note_x = xoffset(ni.start());
            m_note_y = total_height() - ((ni.note() + 1) * noteheight);

            /*
             * Orange note if selected, red for drum mode.
             */

            if (ni.selected())
                brush.setColor(sel_color());
            else
                brush.setColor(drum_paint());

            pen.setColor(fore_color());
            painter.setPen(pen);
            painter.setBrush(brush);
            draw_drum_note(painter, m_note_x, m_note_y);
        }
    }
}

int
qseqroll::note_off_length () const
{
    return m_note_length - m_note_off_margin;
}

/**
 * Convenience wrapper for sequence::add_note() and sequence::add_chord().
 * The length parameters is obtained from the note_off_length() function.
 * This sets the note length at a little less than the snap value.
 *
 * \param tick
 *      The time destination of the new note, in pulses.
 *
 * \param note
 *      The pitch destination of the new note.
 *
 * \param paint
 *      If true, repaint to be left with just the inserted event.  The
 *      default is true.  The value of false is useful in inserting a
 *      number of events and saving the repainting until last.  It is a
 *      bit tricky, as the default paint value for sequence::add_note() is
 *      false.
 */

bool
qseqroll::add_painted_note (midipulse tick, int note)
{
    bool result;
    int n = note_off_length();
#if defined SEQ66_SINGLE_NOTE_UNDO
    if (m_chord > 0)
        result = track().push_add_chord(m_chord, tick, n, note);
    else
        result = track().push_add_note(tick, n, note, true /* paint */);
#else
    if (m_chord > 0)
        result = track().add_chord(m_chord, tick, n, note);
    else
        result = track().add_painted_note(tick, n, note, true /* paint */);
#endif

    if (result)
    {
        result = mark_modified();
        set_dirty();
    }
    return result;
}

void
qseqroll::resizeEvent (QResizeEvent * qrep)
{
    QWidget::resizeEvent(qrep);
}

/**
 *  If it was a button press, set values for dragging.
 */

void
qseqroll::mousePressEvent (QMouseEvent * event)
{
    midipulse tick_s, tick_f;
    int note, note_l, norm_x, norm_y, snapped_x, snapped_y;
    snapped_x = norm_x = event->x() - m_keypadding_x;
    snapped_y = norm_y = event->y();
    snap_x(snapped_x);
    snap_y(snapped_y);
    current_y(snapped_y);
    drop_y(snapped_y);                              /* y is always snapped  */
    if (paste())
    {
        convert_xy(snapped_x, snapped_y, tick_s, note);
        track().paste_selected(tick_s, note);
        paste(false);
        setCursor(Qt::ArrowCursor);
        set_dirty();
    }
    else
    {
        bool isctrl = bool(event->modifiers() & Qt::ControlModifier);
        bool lbutton = event->button() == Qt::LeftButton;
        bool rbutton = event->button() == Qt::RightButton;
        bool mbutton = event->button() == Qt::MiddleButton ||
            (lbutton && isctrl);

        if (lbutton)
        {
            current_x(norm_x);
            drop_x(norm_x);                         /* select non-snapped x */
            if (is_drum_mode())
            {
                int dropxadj = drop_x() - unit_height() / 2;    /* padding  */
                convert_xy(dropxadj, drop_y(), tick_s, note);
            }
            else
            {
                convert_xy(drop_x(), drop_y(), tick_s, note);
                tick_f = tick_s;
            }
            m_last_base_note = note;
            if (adding())                           /* painting new notes   */
            {
                eventlist::select selmode = eventlist::select::would_select;
                painting(true);                     /* start paint job      */
                current_x(snapped_x);
                drop_x(snapped_x);                  /* adding, snapped x    */
                convert_xy(drop_x(), drop_y(), tick_s, note);

                /*
                 * Test if a note is already there. Fake select, if so, don't
                 * add, else add a note, length = little less than snap.
                 */

                bool would_select = ! track().select_note_events
                (
                    tick_s, note, tick_s, note, selmode
                );
                if (would_select)
                {
#if ! defined SEQ66_SINGLE_NOTE_UNDO
                    track().push_undo();
#endif
                    (void) add_painted_note(tick_s, note);
                }
            }
            else                                    /* we're selecting anew */
            {
                /*
                 *  In drum mode, we were using "is_onset", but this breaks
                 *  moving the selected drum events. So we leave it at
                 *  "selected".
                 */

                eventlist::select selmode = eventlist::select::selected;
                bool is_selected = track().select_note_events
                (
                    tick_s, note, tick_f, note, selmode
                );
                if (is_selected)
                {
                    if (! isctrl)
                    {
                        moving_init(true);          /* moving; L-click only */
                        set_dirty();
                        if (is_drum_mode())
                        {
                            track().onsets_selected_box
                            (
                                tick_s, note, tick_f, note_l
                            );
                        }
                        else
                            track().selected_box(tick_s, note, tick_f, note_l);

                        convert_tn_box_to_rect
                        (
                            tick_s, tick_f, note, note_l, selection()
                        );

                        int adj_selected_x = selection().x();
                        snap_x(adj_selected_x);
                        move_snap_offset_x(selection().x() - adj_selected_x);
                        current_x(snapped_x);
                        drop_x(snapped_x);
                    }

                    /*
                     * Middle mouse button or left-ctrl click.
                     */

                    bool can_grow = mbutton && ! is_drum_mode();
                    if (can_grow)
                    {
                        growing(true);
                        track().selected_box(tick_s, note, tick_f, note_l);
                        convert_tn_box_to_rect
                        (
                            tick_s, tick_f, note, note_l, selection()
                        );
                    }
                }
                else
                {
                    if (! isctrl)
                    {
                        track().unselect();

                        /*
                         * ca 2022-05-20: why set dirty here?
                         * qseqeditframe64 modifies the track!
                         *
                         * frame64()->set_dirty();
                         */
                    }
                    selmode = is_drum_mode() ?
                        eventlist::select::onset :
                        eventlist::select::select_one ;

                    int numsel = track().select_note_events
                    (
                        tick_s, note, tick_f, note, selmode
                    );
                    if (numsel == 0)    /* none selected, start selection box */
                        selecting(true);
                    else
                        set_dirty();
                }
            }
        }
        if (rbutton)
            set_adding(true);
    }
}

void
qseqroll::mouseReleaseEvent (QMouseEvent * event)
{
    current_x(int(event->x()) - m_keypadding_x);
    current_y(event->y());
    (void) snap_current_y();
    if (moving())
        (void) snap_current_x();

    int delta_x = current_x() - drop_x();
    int delta_y = current_y() - drop_y();
    midipulse delta_tick;
    int delta_note;
    bool lbutton = event->button() == Qt::LeftButton;
    bool rbutton = event->button() == Qt::RightButton;
    bool isctrl = bool(event->modifiers() & Qt::ControlModifier);   /* Ctrl */
    bool mbutton = event->button() == Qt::MiddleButton || (lbutton && isctrl);
    if (lbutton)
    {
        if (selecting())
        {
            midipulse tick_s, tick_f;   /* start and  end of tick window    */
            int note_h, note_l;         /* high and low notes in window     */
            int x, y, w, h;             /* window dimensions                */
            eventlist::select selmode = eventlist::select::selecting;
            rect::xy_to_rect_get        /* copy drop dimensions to xywh     */
            (
                drop_x(), drop_y(), current_x(), current_y(), x, y, w, h
            );
            convert_xy(x, y, tick_s, note_h);
            convert_xy(x + w, y + h, tick_f, note_l);

            /*
             * This breaks the selection of events in drum mode.
             * if (is_drum_mode()) selmode = eventlist::select::onset;
             */

            int numsel = track().select_note_events
            (
                tick_s, note_h, tick_f, note_l, selmode
            );
            if (numsel > 0)
                frame64()->set_dirty();
        }
        if (moving())
        {
            /*
             * Adjust delta x for snap, convert deltas into screen coordinates.
             * Since delta_note and delta_y are of opposite sign, we flip
             * the final result.  delta_y[0] = note[127].
             */

            int note;
            delta_x -= move_snap_offset_x();
            convert_xy(delta_x, current_y(), delta_tick, note);
            if (m_last_base_note >= 0)
            {
                delta_note = note - m_last_base_note;
            }
            else
            {
                convert_xy(delta_x, delta_y, delta_tick, delta_note);
                delta_note = delta_note - (c_notes_count - 1);
            }
            m_last_base_note = (-1);
            if (delta_tick != 0 || delta_note != 0)
            {
                track().move_selected_notes(delta_tick, delta_note);
                set_dirty();
            }
        }
    }
    if (lbutton || mbutton)
    {
        if (growing())
        {
            convert_xy(delta_x, delta_y, delta_tick, delta_note);
            if (event->modifiers() & Qt::ShiftModifier)
                track().stretch_selected(delta_tick);
            else
                track().grow_selected(delta_tick);

            (void) mark_modified();
            set_dirty();
        }
    }
    if (rbutton)
    {
        if (! QApplication::queryKeyboardModifiers().testFlag(Qt::MetaModifier))
        {
            set_adding(false);
            set_dirty();
        }
    }
    clear_action_flags();               /* turn off all the action flags    */
    track().unpaint_all();
    if (is_dirty())                     /* if clicked, something changed    */
        track().set_dirty();
}

/**
 *  Handles a mouse movement, including selection and note-painting.
 */

void
qseqroll::mouseMoveEvent (QMouseEvent * event)
{
    current_x(event->x() - m_keypadding_x);
    current_y(event->y());
    if (moving_init())
    {
        moving_init(false);
        moving(true);
    }
    (void) snap_current_y();

    int note;
    midipulse tick;
    convert_xy(0, current_y(), tick, note);
    m_seqkeys_wid->preview_key(note);
    if (select_action())
    {
        if (drop_action())
            (void) snap_current_x();
    }
    if (painting())
    {
        if (snap_current_x())
        {
            convert_xy(current_x(), current_y(), tick, note);
            (void) add_painted_note(tick, note);
        }
    }
    set_dirty();
}

bool
qseqroll::zoom_key_press (bool shifted, int key)
{
    bool result = false;
    if (shifted)
    {
        if (key == Qt::Key_Z)
        {
            (void) zoom_in();
            result = true;
        }
        else if (key == Qt::Key_V)
        {
            (void) v_zoom_in();
            result = true;
        }
    }
    else
    {
        if (key == Qt::Key_Z)
        {
            (void) zoom_out();
            result = true;
        }
        else if (key == Qt::Key_0)
        {
            if (m_v_zooming)
            {
                (void) reset_v_zoom();
                result = true;
            }
            else
            {
                (void) reset_zoom();
                result = true;
            }
        }
        else if (key == Qt::Key_V)
        {
            (void) v_zoom_out();
            result = true;
        }
    }
    return result;
}

/**
 *  Handles keystrokes for note movement, zoom, and more.  These key names are
 *  located in /usr/include/x86_64-linux-gnu/qt5/QtCore/qnamespace.h (for
 *  Debian/Ubuntu Linux).
 *
 *  We could simplify this a bit by creating a keystroke object.
 *
 *  At first, qseqeditframe gets the keystrokes.  But giving focus to the
 *  qseqroll causes both to get the keystrokes.
 */

void
qseqroll::keyPressEvent (QKeyEvent * event)
{
    int key = event->key();
    bool isctrl = bool(event->modifiers() & Qt::ControlModifier);
    bool isshift = bool(event->modifiers() & Qt::ShiftModifier);
    bool ismeta = bool(event->modifiers() & Qt::MetaModifier);
    bool done = false;
    if (key == Qt::Key_Delete || key == Qt::Key_Backspace)
    {
        if (track().remove_selected())
            done = mark_modified();
    }
    else
    {
        if (perf().is_pattern_playing())
        {
            /*
             * The space and period keystrokes are handled at the top of
             * qseqeditframe64::keyPressEvent(). The zoom keys are repeated
             * here and below as well.  2021-06-29 let's try allowing note
             * movement during playback.
             */

            if (! isctrl)
            {
                done = movement_key_press(key);
                if (done)
                    done = mark_modified();
                else
                    done = zoom_key_press(isshift, key);
            }
        }
        else
        {
            done = movement_key_press(key);
            if (done)
                done = mark_modified();
            else
            {
                if (isctrl)
                {
                    midipulse tick = perf().get_tick();
                    midipulse len = track().get_length();
                    switch (key)
                    {
                    case Qt::Key_Left:

                        done = true;
                        perf().set_tick(tick - snap(), true);   // no reset
                        track().set_last_tick(tick - snap());
                        break;

                    case Qt::Key_Right:

                        done = true;
                        perf().set_tick(tick + snap(), true);   // no reset
                        track().set_last_tick(tick + snap());
                        break;

                    case Qt::Key_Home:

                        done = true;
                        track().set_last_tick(0);   /* sets it to beginning */
                        if (not_nullptr(frame64()))
                            frame64()->scroll_to_tick(0);
                        break;

                    case Qt::Key_End:

                        done = true;
                        track().set_last_tick();    /* sets it to length    */
                        if (not_nullptr(frame64()))
                            frame64()->scroll_to_tick(len);
                        break;

                    case Qt::Key_X:

                        if (track().cut_selected())
                            done = true;
                        break;

                    case Qt::Key_C:

                        done = true;
                        track().copy_selected();
                        break;

                    case Qt::Key_V:

                        done = true;
                        start_paste();
                        setCursor(Qt::CrossCursor);
                        break;

                    case Qt::Key_Z:

                        done = true;
                        if (event->modifiers() & Qt::ShiftModifier)
                        {
                            /*
                             * Doesn't seem to do anything!
                             */

                            track().pop_redo();
                        }
                        else
                            track().pop_undo();

                        if (not_nullptr(frame64()))
                            frame64()->set_dirty();     /* set_dirty()      */
                        break;

                    case Qt::Key_A:

                        done = true;
                        track().select_all();
                        break;

                    case Qt::Key_D:

                        done = true;
                        sequence::clear_clipboard();    /* drop clipboard   */
                        break;

                    case Qt::Key_E:

                        done = true;
                        track().select_by_channel(frame64()->edit_channel());
                        break;

                    case Qt::Key_K:

                        done = true;
                        analyze_seq_notes();
                        break;

                    case Qt::Key_N:

                        done = true;
                        track().select_notes_by_channel(frame64()->edit_channel());
                        break;
                    }
                }
                else
                    done = zoom_key_press(isshift, key);
            }
        }
        if (! is_dirty())
        {
            if (! isctrl && ! isshift && ! ismeta)
            {
                switch (key)
                {
                case Qt::Key_C:

                    if (frame64()->repitch_selected())
                        done = mark_modified();
                    break;

                case Qt::Key_F:

                    if (track().edge_fix())
                        done = mark_modified();
                    break;

                case Qt::Key_O:

                    if (track().set_recording(false, true))     /* toggle   */
                        done = true;
                    break;

                case Qt::Key_P:

                    done = true;
                    set_adding(true);
                    break;

                case Qt::Key_Q:                 /* quantize selected notes  */

                    if (track().push_quantize(EVENT_NOTE_ON, 0, 1, true))
                        done = mark_modified();
                    break;

                case Qt::Key_R:                 /* default jitter == 8      */

                    if (track().randomize_selected_notes())
                        done = mark_modified();
                    break;

                case Qt::Key_T:                 /* tighten selected notes   */
                    if (track().push_quantize(EVENT_NOTE_ON, 0, 2, true))
                        done = mark_modified();
                    break;

                case Qt::Key_U:

                    if (track().remove_unlinked_notes())
                        done = mark_modified();
                    break;

                case Qt::Key_X:

                    done = true;
                    set_adding(false);
                    break;

                case Qt::Key_Equal:

                    set_adding(false);
                    track().verify_and_link(true);          /* with wrap    */
                    break;
                }
            }
        }
    }
    if (done)
        set_dirty();
    else
        QWidget::keyPressEvent(event);
}

bool
qseqroll::movement_key_press (int key)
{
    bool result = false;
    if (track().any_selected_notes())
    {
        if (key == Qt::Key_Left)
        {
            move_selected_notes(-1, 0);
            result = mark_modified();
        }
        else if (key == Qt::Key_Right)
        {
            move_selected_notes(1, 0);
            result = mark_modified();
        }
        else if (key == Qt::Key_Down)
        {
            move_selected_notes(0, 1);
            result = mark_modified();
        }
        else if (key == Qt::Key_Up)
        {
            move_selected_notes(0, -1);
            result = mark_modified();
        }
    }
    return result;
}

/**
 *  Proposed new function to encapsulate the movement of selections even
 *  more fully.  Works with the four arrow keys.
 *
 *  Note that the movement vertically is different for the selection box versus
 *  the notes.  While the movement values are -1, 0, or 1, the differences are
 *  as follows:
 *
 *      -   Selection box vertical movement:
 *          -   -1 is up one note snap.
 *          -   0 is no vertical movement.
 *          -   +1 is down one note snap.
 *      -   Note vertical movement:
 *          -   -1 is down one note.
 *          -   0 is no note vertical movement.
 *          -   +1 is up one note.
 *
 * \param dx
 *      The amount to move the selection box or the selection horizontally.
 *      Values are -1 (left one time snap), 0 (no movement), and +1 (right one
 *      snap).  Obviously values other than +-1 can be used for larger
 *      movement, but the GUI doesn't yet support that ... we could implement
 *      movement by "pages" some day.
 *
 * \param dy
 *      The amount to move the selection box or the selection vertically.  See
 *      the notes above.
 */

void
qseqroll::move_selected_notes (int dx, int dy)
{
    if (paste())
    {
        //// move_selection_box(dx, dy);
    }
    else
    {
        int snap_x = dx * snap();                   /* time-stamp snap  */
        if (track().any_selected_notes())                /* redundant!       */
        {
            int snap_y = -dy;                       /* note pitch snap  */
            track().move_selected_notes(snap_x, snap_y);
        }
        else if (snap_x != 0)
        {
            track().set_last_tick(track().get_last_tick() + snap_x);
        }
    }
}

#if defined USE_GROW_SELECTED_NOTES_FUNCTION

/**
 *  Proposed new function to encapsulate the movement of selections even
 *  more fully.  Currently no one calls this function!
 *
 * \param dx
 *      The amount to grow the selection horizontally.  Values are -1 (left one
 *      time snap), 0 (no stretching), and +1 (right one snap).  Obviously
 *      values other than +-1 can be used for larger stretching, but the GUI
 *      doesn't yet support that.
 */

void
qseqroll::grow_selected_notes (int dx)
{
    if (! paste())
    {
        int snap_x = dx * snap();                   /* time-stamp snap  */
        growing(true);
        track().grow_selected(snap_x);
    }
}

#endif

/**
 *  Provides the base sizing of the piano roll.  If less than the width of the
 *  parent frame, it is increased to that, so that the roll covers the whole
 *  scrolling area.
 */

QSize
qseqroll::sizeHint () const
{
    int w = frame64()->width();
    int h = total_height();
    int len = tix_to_pix(track().get_length());
    if (len < w)
        len = w;

    len += m_keypadding_x;                          /* c_keyboard_padding   */
    return QSize(len, h);
}

/**
 *  Snaps the y pixel to the height of a piano key.
 *
 * \param [in,out] y
 *      The vertical pixel value to be snapped.
 */

void
qseqroll::snap_y (int & y)
{
    y -= y % unit_height();
}

/**
 *  Provides an override to change the mouse "cursor" based on whether adding
 *  notes is active, or not.
 *
 * \param a
 *      The value of the status of adding (e.g. a note).
 */

void
qseqroll::set_adding (bool a)
{
    qseqbase::set_adding(a);
    if (a)
        setCursor(Qt::PointingHandCursor);      /* Qt doesn't have a pencil */
    else
        setCursor(Qt::ArrowCursor);

    frame64()->update_note_entry(a);            /* updates checkable button */
    set_dirty();
}

/**
 *  The current (x, y) drop points are snapped, and the pasting flag is set to
 *  true.  Then this function Gets the box that selected elements are in, then
 *  adjusts for the clipboard being shifted to tick 0.
 */

void
qseqroll::start_paste ()
{
    (void) snap_current_x();
    (void) snap_current_y();
    drop_x(current_x());
    drop_y(current_y());
    paste(true);

    midipulse tick_s, tick_f;
    int note_h, note_l;
    track().clipboard_box(tick_s, note_h, tick_f, note_l);
    convert_tn_box_to_rect(tick_s, tick_f, note_h, note_l, selection());
    selection().xy_incr(drop_x(), drop_y() - selection().y());
}

/**
 *  Sets the drum/note mode status.
 *
 * \param mode
 *      The drum or note mode status.
 */

void
qseqroll::update_edit_mode (sequence::editmode mode)
{
    m_edit_mode = mode;
}

/**
 *  Sets the current chord to the given value.
 *
 * \param chord
 *      The desired chord value.
 */

void
qseqroll::set_chord (int chord)
{
    if (m_chord != chord)
    {
        m_chord = chord;
        if (is_initialized())
            set_dirty();
    }
}

void
qseqroll::set_key (int key)
{
    if (m_key != key)
    {
        m_key = key;
        if (is_initialized())
            set_dirty();
    }
}

void
qseqroll::set_scale (int scale)
{
    if (int(m_scale) != scale)
    {
        m_scale = static_cast<scales>(scale);
        if (is_initialized())
            set_dirty();
    }
}

void
qseqroll::analyze_seq_notes ()
{
    std::vector<keys> outkeys;
    std::vector<scales> outscales;
    int results = analyze_notes(track().events(), outkeys, outscales);
    if (results > 0)
    {
        std::string message;
        for (int r = 0; r < results; ++r)
        {
            int k = static_cast<int>(outkeys[r]);
            int s = static_cast<int>(outscales[r]);
            char temp[80];
            snprintf
            (
                temp, sizeof temp, "Analysis %d: Key %s, Scale '%s'\n",
                r + 1, musical_key_name(k).c_str(),
                musical_scale_name(s).c_str()
            );
            message += temp;
        }
        if (not_nullptr(m_analysis_msg))
            delete m_analysis_msg;

        m_analysis_msg = new QMessageBox(this);
        m_analysis_msg->setWindowTitle("Estimated Scale(s)");
        m_analysis_msg->setText(qt(message));
        m_analysis_msg->setModal(false);
        m_analysis_msg->show();
    }
}

/**
 *  Checks the position of the tick, and, if it is in a different piano-roll
 *  "page" than the last page, moves the page to the next page.
 *
 *  We don't want to do any of this if the length of the sequence fits in the
 *  window, but for now it doesn't hurt; the progress bar just never meets the
 *  criterion for moving to the next page.
 *
 * \todo
 *      -   If playback is disabled (such as by a trigger), then do not update
 *          the page;
 *      -   When it comes back, make sure we're on the correct page;
 *      -   When it stops, put the window back to the beginning, even if the
 *          beginning is not defined as "0".
 */

void
qseqroll::follow_progress ()
{
    if (not_nullptr(frame64()))
        frame64()->follow_progress();
}

}           // namespace seq66

/*
 * qseqroll.cpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

