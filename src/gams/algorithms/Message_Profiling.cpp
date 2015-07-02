/**
 * Copyright (c) 2015 Carnegie Mellon University. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following acknowledgments and disclaimers.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. The names "Carnegie Mellon University," "SEI" and/or "Software
 *    Engineering Institute" shall not be used to endorse or promote products
 *    derived from this software without prior written permission. For written
 *    permission, please contact permission@sei.cmu.edu.
 * 
 * 4. Products derived from this software may not be called "SEI" nor may "SEI"
 *    appear in their names without prior written permission of
 *    permission@sei.cmu.edu.
 * 
 * 5. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 * 
 *      This material is based upon work funded and supported by the Department
 *      of Defense under Contract No. FA8721-05-C-0003 with Carnegie Mellon
 *      University for the operation of the Software Engineering Institute, a
 *      federally funded research and development center. Any opinions,
 *      findings and conclusions or recommendations expressed in this material
 *      are those of the author(s) and do not necessarily reflect the views of
 *      the United States Department of Defense.
 * 
 *      NO WARRANTY. THIS CARNEGIE MELLON UNIVERSITY AND SOFTWARE ENGINEERING
 *      INSTITUTE MATERIAL IS FURNISHED ON AN "AS-IS" BASIS. CARNEGIE MELLON
 *      UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER EXPRESSED OR
 *      IMPLIED, AS TO ANY MATTER INCLUDING, BUT NOT LIMITED TO, WARRANTY OF
 *      FITNESS FOR PURPOSE OR MERCHANTABILITY, EXCLUSIVITY, OR RESULTS
 *      OBTAINED FROM USE OF THE MATERIAL. CARNEGIE MELLON UNIVERSITY DOES
 *      NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT TO FREEDOM FROM PATENT,
 *      TRADEMARK, OR COPYRIGHT INFRINGEMENT.
 * 
 *      This material has been approved for public release and unlimited
 *      distribution.
 **/

#include "gams/algorithms/Message_Profiling.h"

#include <iostream>
#include <sstream>

using std::stringstream;
using std::string;
using std::map;
using std::cerr;
using std::endl;

const string gams::algorithms::Message_Profiling::key_prefix_ = "message_profiling";

gams::algorithms::Base_Algorithm *
gams::algorithms::Message_Profiling_Factory::create (
  const Madara::Knowledge_Vector & args,
  Madara::Knowledge_Engine::Knowledge_Base * knowledge,
  platforms::Base_Platform * platform,
  variables::Sensors * sensors,
  variables::Self * self,
  variables::Devices * /*devices*/)
{
  Base_Algorithm * result (0);

  // set defaults
  Madara::Knowledge_Record send_size (Madara::Knowledge_Record::Integer (1000));

  if (knowledge && sensors && self)
  {
    if (args.size () >= 1)
      send_size = args[0];

    //if (send_size.is_integer_type ())
      result = new Message_Profiling (send_size, knowledge, platform, sensors, self);
  }

  return result;
}

gams::algorithms::Message_Profiling::Message_Profiling (
  const Madara::Knowledge_Record& send, 
  Madara::Knowledge_Engine::Knowledge_Base * knowledge,
  platforms::Base * platform,
  variables::Sensors * sensors,
  variables::Self * self)
  : Base_Algorithm (knowledge, platform, sensors, self), 
    send_size_ (20)
{
  status_.init_vars (*knowledge, "message_profiling");

  // attach filter
  //knowledge->close_transport ();

  Madara::Transport::QoS_Transport_Settings settings;

//  settings.hosts.push_back (send.to_string ());
//  settings.type = Madara::Transport::BROADCAST;

//  const std::string default_broadcast ("128.237.127.255:15000");
//  settings.hosts.push_back (default_broadcast);
//  settings.type = Madara::Transport::BROADCAST;

  const std::string default_multicast ("239.255.0.1:4150");
  settings.hosts.push_back (default_multicast);
  settings.type = Madara::Transport::MULTICAST;

  //settings.set_send_bandwidth_limit (-1);
  //settings.set_total_bandwidth_limit (-1);

  settings.add_receive_filter (&filter_);

  knowledge->attach_transport (knowledge->get_id (), settings);

  const static string key = key_prefix_ + "." +
    knowledge_->get (".id").to_string () + ".data";

  message_.set_name (key, *knowledge);
}

gams::algorithms::Message_Profiling::~Message_Profiling ()
{
  cerr << filter_.missing_messages_string () << endl;
}

void
gams::algorithms::Message_Profiling::operator= (
  const Message_Profiling & rhs)
{
  if (this != &rhs)
  {
    this->platform_ = rhs.platform_;
    this->sensors_ = rhs.sensors_;
    this->self_ = rhs.self_;
    this->status_ = rhs.status_;
  }
}

int
gams::algorithms::Message_Profiling::analyze (void)
{
  return OK;
}

int
gams::algorithms::Message_Profiling::execute (void)
{
  ++executions_;

  if (send_size_ != 0)
  {
    // key: "message_profiler.{.id}.data"
    // value: "<counter>," + arbitrary data of size send_size_
    stringstream value;
    value << executions_ << ",";
    string value_str = value.str ();
    if(value_str.length () + 1 < send_size_) // fill to size of send_size_
      value_str = value_str + string (send_size_ - value_str.length () - 1, 'a');
  
    // actually set knowledge
    message_ = value_str;
  }

  return 0;
}


int
gams::algorithms::Message_Profiling::plan (void)
{
  return 0;
}

gams::algorithms::Message_Profiling::Message_Filter::~Message_Filter ()
{
}

void
gams::algorithms::Message_Profiling::Message_Filter::filter (
  Madara::Knowledge_Map& records, 
  const Madara::Transport::Transport_Context& transport_context, 
  Madara::Knowledge_Engine::Variables& var)
{
  // get/construct data struct
  string origin = transport_context.get_originator ();
  if (msg_map_.find (origin) == msg_map_.end ())
  {
    msg_map_[origin] = Message_Data(origin, var);
    cerr << "creating Message_Data for " << origin << endl;
  }
  Message_Data& data = msg_map_[origin];

  // loop through each update
  for (Madara::Knowledge_Map::const_reference update : records)
  {
    // we only care about specific messages
    if (update.second.is_string_type () && update.first.find (key_prefix_) == 0)
    {
      // get msg number
      size_t msg_num;
      stringstream (update.second.to_string ()) >> msg_num;

      // is this the first?
      if (data.first == -1)
      {
        data.first = msg_num;
        data.last = msg_num;
      }
      // is this a previously missing message?
      else if (msg_num < data.last.to_integer ())
      {
        // is this earlier than first?
        if (msg_num < data.first.to_integer ())
          data.first = msg_num;
      }
      else // this could only be a new message past last
      {
        data.last = msg_num;
      }

      if (data.present.size() <= msg_num)
        data.present.reserve (msg_num * 2);
      data.present [msg_num] = true;

      Madara::Knowledge_Record::Integer expected = 
        data.first.to_integer () - data.last.to_integer () + 1;
      data.percent_missing = data.present.size() / double (expected);
    }
  }
}

string
gams::algorithms::Message_Profiling::Message_Filter::missing_messages_string ()
  const
{
  stringstream ret_val;
  for (map<string, Message_Data>::const_reference ref : msg_map_)
  {
    ret_val << ref.first << ": ";
    for (size_t i = ref.second.first.to_integer () + 1;
        i < ref.second.last.to_integer (); ++i)
    {
      if(!ref.second.present[i])
        ret_val << i << ",";
    }
    ret_val << endl;
  }
  return ret_val.str();
}

gams::algorithms::Message_Profiling::Message_Filter::Message_Data::Message_Data
  ()
{
  // don't care, whatever goes here will be overwritten by the paramterized 
  // constructor anyway
}

gams::algorithms::Message_Profiling::Message_Filter::Message_Data::Message_Data 
  (string id, Madara::Knowledge_Engine::Variables& var)
{
  string prefix (string (".") + key_prefix_ + "." + id + ".");
  first.set_name (prefix + "first", var);
  first = -1;
  last.set_name (prefix + "last", var);
  percent_missing.set_name (prefix + "percent_missing", var);
}
