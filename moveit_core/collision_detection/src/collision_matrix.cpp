/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Ioan Sucan, E. Gil Jones */

#include <moveit/collision_detection/collision_matrix.h>
#include <boost/bind.hpp>
#include <iomanip>

collision_detection::AllowedCollisionMatrix::AllowedCollisionMatrix()
{
}

collision_detection::AllowedCollisionMatrix::AllowedCollisionMatrix(const std::vector<std::string>& names, bool allowed)
{
  for (std::size_t i = 0; i < names.size(); ++i)
    for (std::size_t j = i; j < names.size(); ++j)
      setEntry(names[i], names[j], allowed);
}

collision_detection::AllowedCollisionMatrix::AllowedCollisionMatrix(const moveit_msgs::AllowedCollisionMatrix& msg)
{
  if (msg.entry_names.size() != msg.entry_values.size() ||
      msg.default_entry_names.size() != msg.default_entry_values.size())
    logError("The number of links does not match the number of entries in AllowedCollisionMatrix message");
  else
  {
    for (std::size_t i = 0; i < msg.entry_names.size(); ++i)
      if (msg.entry_values[i].enabled.size() != msg.entry_names.size())
        logError("Number of entries is incorrect for link '%s' in AllowedCollisionMatrix message",
                 msg.entry_names[i].c_str());
      else
        for (std::size_t j = i + 1; j < msg.entry_values[i].enabled.size(); ++j)
          setEntry(msg.entry_names[i], msg.entry_names[j], msg.entry_values[i].enabled[j]);

    for (std::size_t i = 0; i < msg.default_entry_names.size(); ++i)
      setDefaultEntry(msg.default_entry_names[i], msg.default_entry_values[i]);
  }
}

collision_detection::AllowedCollisionMatrix::AllowedCollisionMatrix(const AllowedCollisionMatrix& acm)
{
  entries_ = acm.entries_;
  allowed_contacts_ = acm.allowed_contacts_;
  default_entries_ = acm.default_entries_;
  default_allowed_contacts_ = acm.default_allowed_contacts_;
}

bool collision_detection::AllowedCollisionMatrix::getEntry(const std::string& name1, const std::string& name2,
                                                           DecideContactFn& fn) const
{
  std::map<std::string, std::map<std::string, DecideContactFn> >::const_iterator it1 = allowed_contacts_.find(name1);
  if (it1 == allowed_contacts_.end())
    return false;
  std::map<std::string, DecideContactFn>::const_iterator it2 = it1->second.find(name2);
  if (it2 == it1->second.end())
    return false;
  fn = it2->second;
  return true;
}

bool collision_detection::AllowedCollisionMatrix::getEntry(const std::string& name1, const std::string& name2,
                                                           AllowedCollision::Type& allowed_collision) const
{
  std::map<std::string, std::map<std::string, AllowedCollision::Type> >::const_iterator it1 = entries_.find(name1);
  if (it1 == entries_.end())
    return false;
  std::map<std::string, AllowedCollision::Type>::const_iterator it2 = it1->second.find(name2);
  if (it2 == it1->second.end())
    return false;
  allowed_collision = it2->second;
  return true;
}

bool collision_detection::AllowedCollisionMatrix::hasEntry(const std::string& name) const
{
  return entries_.find(name) != entries_.end();
}

bool collision_detection::AllowedCollisionMatrix::hasEntry(const std::string& name1, const std::string& name2) const
{
  std::map<std::string, std::map<std::string, AllowedCollision::Type> >::const_iterator it1 = entries_.find(name1);
  if (it1 == entries_.end())
    return false;
  std::map<std::string, AllowedCollision::Type>::const_iterator it2 = it1->second.find(name2);
  if (it2 == it1->second.end())
    return false;
  return true;
}

void collision_detection::AllowedCollisionMatrix::setEntry(const std::string& name1, const std::string& name2,
                                                           bool allowed)
{
  const AllowedCollision::Type v = allowed ? AllowedCollision::ALWAYS : AllowedCollision::NEVER;
  entries_[name1][name2] = entries_[name2][name1] = v;

  // remove boost::function pointers, if any
  std::map<std::string, std::map<std::string, DecideContactFn> >::iterator it = allowed_contacts_.find(name1);
  if (it != allowed_contacts_.end())
  {
    std::map<std::string, DecideContactFn>::iterator jt = it->second.find(name2);
    if (jt != it->second.end())
      it->second.erase(jt);
  }
  it = allowed_contacts_.find(name2);
  if (it != allowed_contacts_.end())
  {
    std::map<std::string, DecideContactFn>::iterator jt = it->second.find(name1);
    if (jt != it->second.end())
      it->second.erase(jt);
  }
}

void collision_detection::AllowedCollisionMatrix::setEntry(const std::string& name1, const std::string& name2,
                                                           const DecideContactFn& fn)
{
  entries_[name1][name2] = entries_[name2][name1] = AllowedCollision::CONDITIONAL;
  allowed_contacts_[name1][name2] = allowed_contacts_[name2][name1] = fn;
}

void collision_detection::AllowedCollisionMatrix::removeEntry(const std::string& name)
{
  entries_.erase(name);
  allowed_contacts_.erase(name);
  for (std::map<std::string, std::map<std::string, AllowedCollision::Type> >::iterator it = entries_.begin();
       it != entries_.end(); ++it)
    it->second.erase(name);
  for (std::map<std::string, std::map<std::string, DecideContactFn> >::iterator it = allowed_contacts_.begin();
       it != allowed_contacts_.end(); ++it)
    it->second.erase(name);
}

void collision_detection::AllowedCollisionMatrix::removeEntry(const std::string& name1, const std::string& name2)
{
  std::map<std::string, std::map<std::string, AllowedCollision::Type> >::iterator jt = entries_.find(name1);
  if (jt != entries_.end())
  {
    std::map<std::string, AllowedCollision::Type>::iterator it = jt->second.find(name2);
    if (it != jt->second.end())
      jt->second.erase(it);
  }
  jt = entries_.find(name2);
  if (jt != entries_.end())
  {
    std::map<std::string, AllowedCollision::Type>::iterator it = jt->second.find(name1);
    if (it != jt->second.end())
      jt->second.erase(it);
  }

  std::map<std::string, std::map<std::string, DecideContactFn> >::iterator it = allowed_contacts_.find(name1);
  if (it != allowed_contacts_.end())
  {
    std::map<std::string, DecideContactFn>::iterator jt = it->second.find(name2);
    if (jt != it->second.end())
      it->second.erase(jt);
  }
  it = allowed_contacts_.find(name2);
  if (it != allowed_contacts_.end())
  {
    std::map<std::string, DecideContactFn>::iterator jt = it->second.find(name1);
    if (jt != it->second.end())
      it->second.erase(jt);
  }
}

void collision_detection::AllowedCollisionMatrix::setEntry(const std::string& name,
                                                           const std::vector<std::string>& other_names, bool allowed)
{
  for (std::size_t i = 0; i < other_names.size(); ++i)
    if (other_names[i] != name)
      setEntry(other_names[i], name, allowed);
}

void collision_detection::AllowedCollisionMatrix::setEntry(const std::vector<std::string>& names1,
                                                           const std::vector<std::string>& names2, bool allowed)
{
  for (std::size_t i = 0; i < names1.size(); ++i)
    setEntry(names1[i], names2, allowed);
}

void collision_detection::AllowedCollisionMatrix::setEntry(const std::string& name, bool allowed)
{
  std::string last = name;
  for (std::map<std::string, std::map<std::string, AllowedCollision::Type> >::iterator it = entries_.begin();
       it != entries_.end(); ++it)
    if (name != it->first && last != it->first)
    {
      last = it->first;
      setEntry(name, it->first, allowed);
    }
}

void collision_detection::AllowedCollisionMatrix::setEntry(bool allowed)
{
  const AllowedCollision::Type v = allowed ? AllowedCollision::ALWAYS : AllowedCollision::NEVER;
  for (std::map<std::string, std::map<std::string, AllowedCollision::Type> >::iterator it1 = entries_.begin();
       it1 != entries_.end(); ++it1)
    for (std::map<std::string, AllowedCollision::Type>::iterator it2 = it1->second.begin(); it2 != it1->second.end();
         ++it2)
      it2->second = v;
}

void collision_detection::AllowedCollisionMatrix::setDefaultEntry(const std::string& name, bool allowed)
{
  const AllowedCollision::Type v = allowed ? AllowedCollision::ALWAYS : AllowedCollision::NEVER;
  default_entries_[name] = v;
  default_allowed_contacts_.erase(name);
}

void collision_detection::AllowedCollisionMatrix::setDefaultEntry(const std::string& name, const DecideContactFn& fn)
{
  default_entries_[name] = AllowedCollision::CONDITIONAL;
  default_allowed_contacts_[name] = fn;
}

bool collision_detection::AllowedCollisionMatrix::getDefaultEntry(const std::string& name,
                                                                  AllowedCollision::Type& allowed_collision) const
{
  std::map<std::string, AllowedCollision::Type>::const_iterator it = default_entries_.find(name);
  if (it == default_entries_.end())
    return false;
  allowed_collision = it->second;
  return true;
}

bool collision_detection::AllowedCollisionMatrix::getDefaultEntry(const std::string& name, DecideContactFn& fn) const
{
  std::map<std::string, DecideContactFn>::const_iterator it = default_allowed_contacts_.find(name);
  if (it == default_allowed_contacts_.end())
    return false;
  fn = it->second;
  return true;
}

namespace collision_detection
{
static bool andDecideContact(const DecideContactFn& f1, const DecideContactFn& f2,
                             collision_detection::Contact& contact)
{
  return f1(contact) && f2(contact);
}
}

bool collision_detection::AllowedCollisionMatrix::getAllowedCollision(const std::string& name1,
                                                                      const std::string& name2,
                                                                      DecideContactFn& fn) const
{
  DecideContactFn fn1, fn2;
  bool found1 = getDefaultEntry(name1, fn1);
  bool found2 = getDefaultEntry(name2, fn2);

  if (!found1 && !found2)
    return getEntry(name1, name2, fn);
  else
  {
    if (found1 && !found2)
      fn = fn1;
    else if (!found1 && found2)
      fn = fn2;
    else if (found1 && found2)
      fn = boost::bind(&andDecideContact, fn1, fn2, _1);
    else
      return false;
    return true;
  }
}

bool collision_detection::AllowedCollisionMatrix::getAllowedCollision(const std::string& name1,
                                                                      const std::string& name2,
                                                                      AllowedCollision::Type& allowed_collision) const
{
  AllowedCollision::Type t1, t2;
  bool found1 = getDefaultEntry(name1, t1);
  bool found2 = getDefaultEntry(name2, t2);

  if (!found1 && !found2)
    return getEntry(name1, name2, allowed_collision);
  else
  {
    if (found1 && !found2)
      allowed_collision = t1;
    else if (!found1 && found2)
      allowed_collision = t2;
    else if (found1 && found2)
    {
      if (t1 == AllowedCollision::NEVER || t2 == AllowedCollision::NEVER)
        allowed_collision = AllowedCollision::NEVER;
      else if (t1 == AllowedCollision::CONDITIONAL || t2 == AllowedCollision::CONDITIONAL)
        allowed_collision = AllowedCollision::CONDITIONAL;
      else  // ALWAYS is the only remaining case
        allowed_collision = AllowedCollision::ALWAYS;
    }
    else
      return false;
    return true;
  }
}

void collision_detection::AllowedCollisionMatrix::clear()
{
  entries_.clear();
  allowed_contacts_.clear();
  default_entries_.clear();
  default_allowed_contacts_.clear();
}

void collision_detection::AllowedCollisionMatrix::getAllEntryNames(std::vector<std::string>& names) const
{
  names.clear();
  for (std::map<std::string, std::map<std::string, AllowedCollision::Type> >::const_iterator it = entries_.begin();
       it != entries_.end(); ++it)
    if (!names.empty() && names.back() == it->first)
      continue;
    else
      names.push_back(it->first);
}

void collision_detection::AllowedCollisionMatrix::getMessage(moveit_msgs::AllowedCollisionMatrix& msg) const
{
  msg.entry_names.clear();
  msg.entry_values.clear();
  msg.default_entry_names.clear();
  msg.default_entry_values.clear();

  getAllEntryNames(msg.entry_names);
  std::sort(msg.entry_names.begin(), msg.entry_names.end());

  msg.entry_values.resize(msg.entry_names.size());
  for (std::size_t i = 0; i < msg.entry_names.size(); ++i)
    msg.entry_values[i].enabled.resize(msg.entry_names.size(), false);

  // there is an approximation here: if we use a boost function to decide
  // whether a collision is allowed or not, we just assume the collision is not allowed.
  for (std::size_t i = 0; i < msg.entry_names.size(); ++i)
  {
    AllowedCollision::Type dtype;
    bool dfound = getDefaultEntry(msg.entry_names[i], dtype);
    if (dfound)
    {
      msg.default_entry_names.push_back(msg.entry_names[i]);
      msg.default_entry_values.push_back(dtype == AllowedCollision::ALWAYS);
    }

    for (std::size_t j = i; j < msg.entry_names.size(); ++j)
    {
      AllowedCollision::Type type;
      bool found = getEntry(msg.entry_names[i], msg.entry_names[j], type);
      if (found)
        msg.entry_values[i].enabled[j] = msg.entry_values[j].enabled[i] = type == AllowedCollision::ALWAYS;
    }
  }
}

void collision_detection::AllowedCollisionMatrix::print(std::ostream& out) const
{
  std::vector<std::string> names;
  getAllEntryNames(names);
  std::sort(names.begin(), names.end());

  std::size_t L = 4;
  for (std::size_t i = 0; i < names.size(); ++i)
  {
    std::size_t l = names[i].length();
    if (l > L)
      L = l;
  }
  ++L;

  std::size_t D = 2;
  while (names.size() > pow(10, D) - 1)
    D++;

  // print indices along the top of the matrix
  for (std::size_t j = 0; j < D; ++j)
  {
    out << std::setw(L + D + 4) << "";
    for (std::size_t i = 0; i < names.size(); ++i)
    {
      std::stringstream ss;
      ss << std::setw(D) << i;
      out << std::setw(3) << ss.str().c_str()[j];
    }
    out << std::endl;
  }

  for (std::size_t i = 0; i < names.size(); ++i)
  {
    out << std::setw(L) << names[i];
    out << std::setw(D + 1) << i;
    out << " | ";
    for (std::size_t j = 0; j < names.size(); ++j)
    {
      AllowedCollision::Type type;
      bool found = getAllowedCollision(names[i], names[j], type);
      if (found)
        out << std::setw(3) << (type == AllowedCollision::ALWAYS ? '1' : (type == AllowedCollision::NEVER ? '0' : '?'));
      else
        out << std::setw(3) << '-';
    }
    out << std::endl;
  }
}
