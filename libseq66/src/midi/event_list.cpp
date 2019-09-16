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
 * \file          event_list.cpp
 *
 *  This module declares/defines a class for handling MIDI events in a list
 *  container.
 *
 * \library       seq66 application
 * \author        Chris Ahlstrom
 * \date          2015-09-19
 * \updates       2019-09-12
 * \license       GNU GPLv2 or above
 *
 *  This container now can indicate if certain Meta events (time-signaure or
 *  tempo) have been added to the container.
 *
 *  This module also defines the  event_list::event_key object.  Although the
 *  main MIDI container are now back to using std::list (with sorting after
 *  loading).
 */

#include <stdio.h>                      /* C::printf()                  */

#include "util/basic_macros.hpp"
#include "midi/event_list.hpp"

/*
 *  Do not document a namespace; it breaks Doxygen.
 */

namespace seq66
{

/**
 *  Principal event_key constructor.
 *
 * \param tstamp
 *      The time-stamp is the primary part of the key.  It is the most
 *      important key item.
 *
 * \param rank
 *      Rank is an arbitrary number used to prioritize events that have the
 *      same time-stamp.  See the event::get_rank() function for more
 *      information.
 */

event_list::event_key::event_key (midipulse tstamp, int rank)
 :
    m_timestamp (tstamp),
    m_rank      (rank)
{
    // Empty body
}

/**
 *  Event-based constructor.  This constructor makes it even easier to
 *  create an event_key.  Note that the call to event::get_rank() makes a
 *  simple calculation based on the status of the event.
 *
 * \param rhs
 *      Provides the event key to be copied.
 */

event_list::event_key::event_key (const event & rhs)
 :
    m_timestamp (rhs.timestamp()),
    m_rank      (rhs.get_rank())
{
    // Empty body
}

/**
 *  Provides the minimal operator needed to sort events using an event_key.
 *
 * \param rhs
 *      Provides the event key to be compared against.
 *
 * \return
 *      Returns true if the rank and timestamp of the current object are less
 *      than those of rhs.
 */

bool
event_list::event_key::operator < (const event_key & rhs) const
{
    if (m_timestamp == rhs.m_timestamp)
        return (m_rank < rhs.m_rank);
    else
        return (m_timestamp < rhs.m_timestamp);
}

/*
 * Section: event_list
 */

/**
 *  Principal constructor.
 */

event_list::event_list ()
 :
    m_events                (),
    m_is_modified           (false),
    m_has_tempo             (false),
    m_has_time_signature    (false)
{
    // No code needed
}

/**
 *  Copy constructor.
 *
 * \param rhs
 *      Provides the event list to be copied.
 */

event_list::event_list (const event_list & rhs)
 :
    m_events                (rhs.m_events),
    m_is_modified           (rhs.m_is_modified),
    m_has_tempo             (rhs.m_has_tempo),
    m_has_time_signature    (rhs.m_has_time_signature)
{
    // No code needed
}

/**
 *  Principal assignment operator.  Follows the stock rules for such an
 *  operator, just assigning member values.
 *
 * \param rhs
 *      Provides the event list to be assigned.
 */

event_list &
event_list::operator = (const event_list & rhs)
{
    if (this != &rhs)
    {
        m_events                = rhs.m_events;
        m_is_modified           = rhs.m_is_modified;
        m_has_tempo             = rhs.m_has_tempo;
        m_has_time_signature    = rhs.m_has_time_signature;
    }
    return *this;
}

/**
 *  Provides the length of the events in MIDI pulses.  This function gets the
 *  iterator for the last element and returns its length value.
 *
 *  TODO: Don't we need to add the length of the event to the result, it it has
 *  a length???
 *
 * \return
 *      Returns the timestamp of the latest event in the container.
 */

midipulse
event_list::get_length () const
{
    midipulse result = 0;
    if (count() > 0)
    {
        auto lci = m_events.rbegin();                   /* get last element */
        result = lci->timestamp();                      /* get length value */
    }
    return result;
}

/**
 *  Adds an event to the internal event list without sorting.  It is a
 *  wrapper, wrapper for insert() or push_front(), with an option to call
 *  sort().
 *
 *  The add() function without sorting, useful to speed up the initial
 *  container loading into the event-list.
 *
 *  For the std::multimap implementation, This is an option if we want to make
 *  sure the insertion succeed.
 *
 *  If the std::list implementation has been built in, then the event list is
 *  not sorted after the addition.  This is a time-consuming operation.
 *
 *  We also have to raise some new flags if the event is a Set Tempo or
 *  Time Signature event, so that we do not force the current tempo and
 *  time-signature when writing the MIDI file.
 *
 * \warning
 *      This pushing (and, in writing the MIDI file, the popping),
 *      causes events with identical timestamps to be written in
 *      reverse order.  Doesn't affect functionality, but it's puzzling
 *      until one understands what is happening.  That's why we're
 *      now preferring to use an std::multimap as the container.  But see the
 *      new news on using std::vector.
 *
 * \param e
 *      Provides the event to be added to the list.
 *
 * \return
 *      Returns true.  We assume the insertion succeeded, and no longer care
 *      about an increment in container size.  It's a multimap, so it always
 *      inserts, and if we don't have memory left, all bets are off anyway.
 */

bool
event_list::append (const event & e)
{
    m_events.push_back(e);              /* std::vector operation        */
    m_is_modified = true;
    if (e.is_tempo())
        m_has_tempo = true;

    if (e.is_time_signature())
        m_has_time_signature = true;

    return true;
}

/**
 *  Provides a merge operation for the event multimap analogous to the merge
 *  operation for the event list.  We have certain constraints to preserve, as
 *  the following discussion shows.
 *
 *  For std::list, sequence merges list T into list A by first calling
 *  T.sort(), and then A.merge(T).  The merge() operation merges T into A by
 *  transferring all of its elements, at their respective ordered positions,
 *  into A.  Both containers must already be ordered.
 *
 *  The merge effectively removes all the elements in T (which becomes empty),
 *  and inserts them into their ordered position within container (which
 *  expands in size by the number of elements transferred). The operation is
 *  performered without constructing nor destroying any element, whether T is an
 *  lvalue or an rvalue, or whether the value-type supports move-construction
 *  or not.
 *
 *  Each element of T is inserted at the position that corresponds to its
 *  value according to the strict weak ordering defined by operator <. The
 *  resulting order of equivalent elements is stable (i.e. equivalent elements
 *  preserve the relative order they had before the call, and existing
 *  elements precede those equivalent inserted from x).  The function does
 *  nothing if (&x == this).
 *
 *  For std::multimap, sorting is automatic.  However, unless
 *  move-construction is supported, merging will be less efficient than for
 *  the list (now a vector) version.
 *
 * \param el
 *      Provides the event list to be merged into the current event list.
 *
 * \param presort
 *      If true, the events are presorted.  This is a requirement for merging
 *      an std::list or std::vector, but is a no-op for the std::multimap
 *      implementation [which no longer exists].
 */

void
event_list::merge (event_list & el, bool presort)
{
#ifdef USE_THIS_CRASHING_CODE
    Events destination;                     /* a vector of MIDI events      */
    if (presort)                            /* should always be true here!  */
    {
        sort();                             /* sort ourselves to be sure    */
        el.sort();                          /* el.m_events.sort();          */
    }
    std::merge
    (
        m_events.begin(), m_events.end(),
        el.m_events.begin(), el.m_events.end(),
        destination.begin()     // std::back_inserter(destination)
    );
    m_events = destination;
#else
    if (presort)                            /* not really necessary here    */
        el.sort();                          /* el.m_events.sort();          */

    m_events.reserve(m_events.size() + el.m_events.size());
    m_events.insert(m_events.end(), el.m_events.begin(), el.m_events.end());
    std::sort(m_events.begin(), m_events.end());    /* event_list::sort()   */
#endif
}

/**
 *  Links a new event.  This function checks for a note on, then look for
 *  its note off.  This function is provided in the event_list because it
 *  does not depend on any external data.  Also note that any desired
 *  thread-safety must be provided by the caller.
 */

void
event_list::link_new ()
{
    bool endfound = false;
    for (auto on = m_events.begin(); on != m_events.end(); ++on)
    {
        event & eon = dref(on);
        if (eon.is_note_on() && ! eon.is_linked())  /* note on, unlinked?   */
        {
            auto off = on;                          /* point to note on     */
            ++off;                                  /* get next element     */
            endfound = false;
            while (off != m_events.end())
            {
                event & eoff = dref(off);
                endfound = link_new_note(eon, eoff);
                if (endfound)
                    break;

                ++off;
            }
            if (! endfound)
            {
                off = m_events.begin();
                while (off != on)
                {
                    event & eoff = dref(off);
                    endfound = link_note(eon, eoff);
                    if (endfound)
                        break;

                    ++off;
                }
            }
        }
    }
}


/**
 *  THINK ABOUT IT:  If we're in legacy merge mode for a loop, the Note Off is
 *  actually earlier than the Note On.  And in replace mode, the Note On is
 *  cleared, leaving us with a dangling Note Off event.
 *
 *  We should consider, in both modes, automatically adding the Note Off at the
 *  end of the loop and ignoring the next note off on the same note from the
 *  keyboard.
 *
 *  Careful!
 */

bool
event_list::link_new_note (event & eon, event & eoff)
{
    bool result =                       /* Off, == notes, not linked    */
    (
        eoff.is_note_off() && eoff.get_note() == eon.get_note() &&
            ! eoff.is_linked()
    );
    if (result)
    {
        eon.link(&eoff);                /* link + mark                  */
        eoff.link(&eon);

        /*
         * Not sure why we bother to mark here, when we unmark them
         * all afterward in preparation for potential pruning.
         *
         *  eon.mark();
         *  eoff.mark();
         */
    }
    return result;
}

/**
 *  The same as link_new_note(), except that it checks is_marked() instead of
 *  is_linked().
 */

bool
event_list::link_note (event & eon, event & eoff)
{
    bool result =                       /* Off, == notes, not marked    */
    (
        eoff.is_note_off() && eoff.get_note() == eon.get_note() &&
            ! eoff.is_marked()
    );
    if (result)
    {
        eon.link(&eoff);                /* link + mark                  */
        eoff.link(&eon);

        /*
         * Not sure why we bother to mark here, when we unmark them
         * all afterward in preparation for potential pruning.
         *
         *  eon.mark();
         *  eoff.mark();
         */
    }
    return result;
}

/**
 *  This function verifies state: all note-ons have an off, and it links
 *  note-offs with their note-ons.
 *
 * Stazed (seq32):
 *
 *      This function now deletes any notes that are >= m_length, so any
 *      resize or move of notes must modify for wrapping if Note Off is >=
 *      m_length.
 *
 * \threadunsafe
 *      As in most case, the caller will use an automutex to call this
 *      function safely.
 *
 * \param slength
 *      Provides the length beyond which events will be pruned.
 */

void
event_list::verify_and_link (midipulse slength)
{
    clear_links();
    sort();                                 /* IMPORTANT!                   */
    for (auto on = m_events.begin(); on != m_events.end(); ++on)
    {
        event & eon = dref(on);
        if (eon.is_note_on())               /* Note On, find its Note Off   */
        {
            auto off = on;                  /* next possible Note Off...    */
            ++off;                          /* ...starting here             */
            bool endfound = false;
            while (off != m_events.end())
            {
                event & eoff = dref(off);
                endfound = link_note(eon, eoff);
                if (endfound)
                    break;

                ++off;
            }
            if (! endfound)
            {
                off = m_events.begin();
                while (off != on)
                {
                    event & eoff = dref(off);
                    endfound = link_note(eon, eoff);
                    if (endfound)
                        break;

                    ++off;
                }
            }
        }
    }
    unmark_all();
    mark_out_of_range(slength);
    (void) remove_marked();                 /* prune out-of-range events    */

    /*
     *  Link the tempos in a separate pass (it makes the logic easier and the
     *  amount of time should be unnoticeable to the user.
     */

    link_tempos();
}

/**
 *  Clears all event links and unmarks them all.
 */

void
event_list::clear_links ()
{
    for (auto & e : m_events)
    {
        e.unmark();
        e.unlink();                     /* used to be e.clear_link()        */
    }
}

#if defined USE_FILL_TIME_SIG_AND_TEMPO

/**
 *  Scans the event-list for any tempo or time_signature events.
 *  The use may have deleted them and is depending on a setting made in the
 *  user-interface.  So we must set/unset the flags before saving.  This check
 *  was added to fix issue #141.
 */

void
event_list::scan_meta_events ()
{
    m_has_tempo = false;
    m_has_time_signature = false;
    for (auto & e : m_events)
    {
        if (e.is_tempo())
            m_has_tempo = true;

        if (e.is_time_signature())
            m_has_time_signature = true;
    }
}

#endif  // USE_FILL_TIME_SIG_AND_TEMPO

/**
 *  This function tries to link tempo events.  Native support for temp tracks
 *  is a new feature of seq66.  These links are only in one direction: forward
 *  in time, to the next tempo event, if any.
 *
 *  Also, at present, tempo events are not markable.
 *
 * \threadunsafe
 *      As in most case, the caller will use an automutex to call this
 *      function safely.
 */

void
event_list::link_tempos ()
{
    clear_tempo_links();
    for (auto t = m_events.begin(); t != m_events.end(); ++t)
    {
        event & e = dref(t);
        if (e.is_tempo())
        {
            auto t2 = t;                    /* next possible Set Tempo...   */
            ++t2;                           /* ...starting here             */
            while (t2 != m_events.end())
            {
                event & et2 = dref(t2);
                if (et2.is_tempo())
                {
                    e.link(&et2);                   /* link + mark          */
                    break;                          /* tempos link one way  */
                }
                ++t2;
            }
        }
    }
}

/**
 *  Clears all tempo event links.
 */

void
event_list::clear_tempo_links ()
{
    for (auto & e : m_events)
    {
        if (e.is_tempo())
            e.unlink();                 /* was e.clear_link()   */
    }
}

/**
 *  Marks all selected events.
 *
 * \return
 *      Returns true if there was even one event selected and marked.
 */

bool
event_list::mark_selected ()
{
    bool result = false;
    for (auto & e : m_events)
    {
        if (e.is_selected())
        {
            e.mark();
            result = true;
        }
    }
    return result;
}

/**
 *  Marks all events.  Not yet used, but might come in handy with the event
 *  editor dialog.
 */

void
event_list::mark_all ()
{
    for (auto & e : m_events)
        e.mark();
}

/**
 *  Unmarks all events.
 */

void
event_list::unmark_all ()
{
    for (auto & e : m_events)
        e.unmark();
}

/**
 *  Marks all events that have a time-stamp that is out of range.
 *  Used for killing (pruning) those events not in range.  If the current
 *  time-stamp is greater than the length, then the event is marked for
 *  pruning.
 *
 * \note
 *      This code was comparing the timestamp as greater than or equal to the
 *      sequence length.  However, being equal is fine.  This may explain why
 *      the midifile code would add one tick to the length of the last note
 *      when processing the end-of-track.
 *
 * \param slength
 *      Provides the length beyond which events will be pruned.
 */

void
event_list::mark_out_of_range (midipulse slength)
{
    for (auto & e : m_events)
    {
        bool prune = e.timestamp() > slength;   /* WAS ">=", SEE BANNER */
        if (! prune)
            prune = e.timestamp() < 0;          /* added back, seq66    */

        if (prune)
        {
            e.mark();
            if (e.is_linked())
                e.link()->mark();
        }
    }
}

/**
 *  Removes marked events.  Note how this function handles removing a
 *  value to avoid incrementing a now-invalid iterator.
 *
 * \threadsafe
 *
 * \return
 *      Returns true if at least one event was removed.
 */

bool
event_list::remove_marked ()
{
    bool result = false;
    auto i = m_events.begin();
    while (i != m_events.end())
    {
        if (dref(i).is_marked())
        {
            auto t = remove(i);
            i = t;
            result = true;
        }
        else
            ++i;
    }
    return result;
}

/**
 *  Unpaints all list-events.
 */

void
event_list::unpaint_all ()
{
    for (auto & e : m_events)
        e.unpaint();
}

/**
 *  Counts the selected note-on events in the event list.
 */

int
event_list::count_selected_notes () const
{
    int result = 0;
    for (auto & e : m_events)
    {
        if (e.is_note_on() && e.is_selected())
            ++result;
    }
    return result;
}

/**
 *  Indicates that at least one note is selected.  Acts like
 *  event_list::count_selected_notes(), but stops after finding a selected
 *  note.
 *
 * \return
 *      Returns true if at least one note is selected.
 */

bool
event_list::any_selected_notes () const
{
    bool result = false;
    for (auto & e : m_events)
    {
        if (e.is_note_on() && e.is_selected())
        {
            result = true;
            break;
        }
    }
    return result;
}

/**
 *  Counts the selected events, with the given status, in the event list.
 *  If the event is a control change (CC), then it must also match the
 *  given CC value.  The one exception is tempo events, which are always
 *  selectable.
 *
 * \param status
 *      The desired status value to count.
 *
 * \param cc
 *      The desired control-change to count.  Used only if the status
 *      parameter indicates a control-change event.
 *
 * \return
 *      Returns the number of selected events.
 */

int
event_list::count_selected_events (midibyte status, midibyte cc) const
{
    int result = 0;
    for (auto & e : m_events)
    {
        if (e.is_tempo())
        {
            if (e.is_selected())
                ++result;
        }
        else if (e.get_status() == status)
        {
            midibyte d0, d1;
            e.get_data(d0, d1);                 /* get the two data bytes */
            if (event::is_desired_cc_or_not_cc(status, cc, d0))
            {
                if (e.is_selected())
                    ++result;
            }
        }
    }
    return result;
}

/**
 *  Indicates that at least one matching event is selected.  Acts like
 *  event_list::count_selected_events(), but stops after finding a selected
 *  note.
 *
 * \return
 *      Returns true if at least one matching event is selected.
 */

bool
event_list::any_selected_events (midibyte status, midibyte cc) const
{
    bool result = false;
    for (auto & e : m_events)
    {
        if (e.is_tempo())
        {
            if (e.is_selected())
            {
                result = true;
                break;
            }
        }
        else if (e.get_status() == status)
        {
            midibyte d0, d1;
            e.get_data(d0, d1);                 /* get the two data bytes */
            if (event::is_desired_cc_or_not_cc(status, cc, d0))
            {
                if (e.is_selected())
                {
                    result = true;
                    break;
                }
            }
        }
    }
    return result;
}

/**
 *  Selects all events, unconditionally.
 */

void
event_list::select_all ()
{
    for (auto & e : m_events)
        e.select();
}

/**
 *  Deselects all events, unconditionally.
 */

void
event_list::unselect_all ()
{
    for (auto & e : m_events)
        e.unselect();
}

/**
 *  Prints a list of the currently-held events.  Useful for debugging.
 */

void
event_list::print () const
{
    printf("events[%d]:\n", count());
    for (auto & e : m_events)
        e.print();
}

}           // namespace seq66

/*
 * event_list.cpp
 *
 * vim: sw=4 ts=4 wm=4 et ft=cpp
 */

