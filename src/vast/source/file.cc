#include <vast/source/file.h>

#include <ze/event.h>
#include <ze/util/parse_helpers.h>
#include "vast/exception.h"
#include "vast/logger.h"

namespace vast {
namespace source {

file::file(cppa::actor_ptr ingestor,
           cppa::actor_ptr tracker,
           std::string const& filename)
  : event_source(std::move(ingestor), std::move(tracker))
  , file_(filename)
{
  LOG(verbose, ingest)
    << "spawning event source @" << id()
    << " for file " << filename;

  if (file_)
  {
    file_.unsetf(std::ios::skipws);
    finished_ = false;
  }
  else
  {
    LOG(error, ingest) << "event source @" << id() << " cannot read " << filename;
  }
}

line::line(cppa::actor_ptr ingestor,
           cppa::actor_ptr tracker,
           std::string const& filename)
  : file(std::move(ingestor), std::move(tracker), filename)
{
  next();
}

ze::event line::extract()
{
  while (line_.empty())
    if (! next())
      break;

  auto event = parse(line_);
  next();
  return event;
}

bool line::next()
{
  auto success = false;
  if (std::getline(file_, line_))
    success = true;

  ++current_line_;
  if (! file_)
    finished_ = true;

  return success;
}
  
bro2::bro2(cppa::actor_ptr ingestor,
           cppa::actor_ptr tracker,
           std::string const& filename)
  : line(std::move(ingestor), std::move(tracker), filename)
{
  parse_header();
}

void bro2::parse_header()
{
  ze::util::field_splitter<std::string::const_iterator> fs;
  fs.split(line_.begin(), line_.end());
  if (fs.fields() != 2 || std::string(fs.start(0), fs.end(0)) != "#separator")
    throw error::parse("invalid #separator definition");

  {
    std::string sep;
    std::string bro_sep(fs.start(1), fs.end(1));
    std::string::size_type pos = 0;
    while (pos != std::string::npos)
    {
      pos = bro_sep.find("\\x", pos);
      if (pos != std::string::npos)
      {
        auto i = std::stoi(bro_sep.substr(pos + 2, 2), nullptr, 16);
        assert(i >= 0 && i <= 255);
        sep.push_back(i);
        pos += 2;
      }
    }

    separator_ = ze::string(sep.begin(), sep.end());
  }

  {
    if (! next())
      throw error::parse("could not extract second log line");

    ze::util::field_splitter<std::string::const_iterator> fs;
    fs.sep(separator_.data(), separator_.size());
    fs.split(line_.begin(), line_.end());
    if (fs.fields() != 2 || std::string(fs.start(0), fs.end(0)) != "#set_separator")
      throw error::parse("invalid #set_separator definition");

    auto set_sep = std::string(fs.start(1), fs.end(1));
    set_separator_ = ze::string(set_sep.begin(), set_sep.end());
  }

  {
    if (! next())
      throw error::parse("could not extract third log line");

    ze::util::field_splitter<std::string::const_iterator> fs;
    fs.sep(separator_.data(), separator_.size());
    fs.split(line_.begin(), line_.end());
    if (fs.fields() != 2 || std::string(fs.start(0), fs.end(0)) != "#empty_field")
      throw error::parse("invalid #empty_field definition");

    auto empty = std::string(fs.start(1), fs.end(1));
    empty_field_ = ze::string(empty.begin(), empty.end());
  }

  {
    if (! next())
      throw error::parse("could not extract fourth log line");

    ze::util::field_splitter<std::string::const_iterator> fs;
    fs.sep(separator_.data(), separator_.size());
    fs.split(line_.begin(), line_.end());
    if (fs.fields() != 2 || std::string(fs.start(0), fs.end(0)) != "#unset_field")
      throw error::parse("invalid #unset_field definition");

    auto unset = std::string(fs.start(1), fs.end(1));
    unset_field_ = ze::string(unset.begin(), unset.end());
  }

  {
    if (! next())
      throw error::parse("could not extract fifth log line");

    ze::util::field_splitter<std::string::const_iterator> fs;
    fs.sep(separator_.data(), separator_.size());
    fs.split(line_.begin(), line_.end());
    if (fs.fields() != 2 || std::string(fs.start(0), fs.end(0)) != "#path")
      throw error::parse("invalid #path definition");

    path_ = ze::string(fs.start(1), fs.end(1));
  }

  {
    if (! next())
      throw error::parse("could not extract sixth log line");

    ze::util::field_splitter<std::string::const_iterator> fs;
    fs.sep(separator_.data(), separator_.size());
    fs.split(line_.begin(), line_.end());

    for (size_t i = 1; i < fs.fields(); ++i)
      field_names_.emplace_back(fs.start(i), fs.end(i));
  }

  {
    if (! next())
      throw error::parse("could not extract seventh log line");

    ze::util::field_splitter<std::string::const_iterator> fs;
    fs.sep(separator_.data(), separator_.size());
    fs.split(line_.begin(), line_.end());

    for (size_t i = 1; i < fs.fields(); ++i)
    {
      ze::string t(fs.start(i), fs.end(i));

      if (t.starts_with("table["))
      {
        field_types_.push_back(ze::set_type);
        ze::string elem(t.find("[") + 1, t.end() - 1);
        set_types_.push_back(bro_to_ze(elem));
      }
      else
      {
        field_types_.push_back(bro_to_ze(t));
      }
    }
  }

  if (file_.peek() == '#')
    throw error::parse("more headers than VAST knows");

  DBG(ingest)
    << "event source @" << id() << " parsed bro2 header:"
    << " #separator " << separator_
    << " #set_separator " << set_separator_
    << " #empty_field " << empty_field_
    << " #unset_field " << unset_field_
    << " #path " << path_;
  {
    std::ostringstream str;
    for (auto& name : field_names_)
      str << " " << name;
    DBG(ingest) << "event source @" << id() << " has field names:" << str.str();
  }
  {
    std::ostringstream str;
    for (auto& type : field_types_)
      str << " " << type;
    DBG(ingest) << "event source @" << id() << " has field types:" << str.str();
  }
  if (! set_types_.empty())
  {
    std::ostringstream str;
    for (auto& type : set_types_)
      str << " " << type;
    DBG(ingest) << "event source @" << id() << " has set types:" << str.str();
  }

  path_ = "bro::" + path_;
  next();
}

ze::value_type bro2::bro_to_ze(ze::string const& type)
{
  if (type == "enum" || type == "string" || type == "file")
    return ze::string_type;
  else if (type == "bool")
    return ze::bool_type;
  else if (type == "int")
    return ze::int_type;
  else if (type == "count")
    return ze::uint_type;
  else if (type == "double")
    return ze::double_type;
  else if (type == "interval")
    return ze::duration_type;
  else if (type == "time")
    return ze::timepoint_type;
  else if (type == "addr")
    return ze::address_type;
  else if (type == "port")
    return ze::port_type;
  else if (type == "pattern")
    return ze::regex_type;
  else if (type == "subnet")
    return ze::prefix_type;
  else
    return ze::invalid_type;
}

ze::event bro2::parse(std::string const& line)
{
  ze::util::field_splitter<std::string::const_iterator> fs;
  fs.sep(separator_.data(), separator_.size());
  fs.split(line.begin(), line.end());
  if (fs.fields() != field_types_.size())
    throw error::parse("inconsistent number of fields");

  ze::event e(path_);
  e.timestamp(ze::clock::now());
  size_t sets = 0;
  for (size_t f = 0; f < fs.fields(); ++f)
  {
    auto start = fs.start(f);
    auto end = fs.end(f);

    auto unset = true;
    for (size_t i = 0; i < unset_field_.size(); ++i)
    {
      if (unset_field_[i] != *(start + i))
      {
        unset = false;
        break;
      }
    }
    if (unset)
    {
      e.push_back(ze::nil);
      continue;
    }

    auto empty = true;
    for (size_t i = 0; i < empty_field_.size(); ++i)
    {
      if (empty_field_[i] != *(start + i))
      {
        empty = false;
        break;
      }
    }
    if (empty)
    {
      e.emplace_back(field_types_[f]);
      continue;
    }

    ze::value v;
    if (field_types_[f] == ze::set_type)
      v = ze::set::parse(set_types_[sets++], start, end, set_separator_);
    else
      v = ze::value::parse(field_types_[f], start, end);
    e.emplace_back(std::move(v));
  }

  return e;
}

bro15conn::bro15conn(cppa::actor_ptr ingestor,
                     cppa::actor_ptr tracker,
                     std::string const& filename)
  : line(std::move(ingestor), std::move(tracker), filename)
{
}

ze::event bro15conn::parse(std::string const& line)
{
  ze::event e("bro::conn");
  e.timestamp(ze::clock::now());

  ze::util::field_splitter<std::string::const_iterator> fs;
  fs.split(line.begin(), line.end(), 13);
  if (! (fs.fields() == 12 || fs.fields() == 13))
    throw error::parse("not enough conn.log fields (at least 12 needed)");

  // Timestamp
  auto i = fs.start(0);
  auto j = fs.end(0);
  e.emplace_back(ze::value::parse(ze::timepoint_type, i, j));
  if (i != j)
    throw error::parse("invalid conn.log timestamp (field 1)");

  // Duration
  i = fs.start(1);
  j = fs.end(1);
  if (*i == '?')
  {
    e.emplace_back(ze::nil);
  }
  else
  {
    e.emplace_back(ze::value::parse(ze::duration_type, i, j));
    if (i != j)
      throw error::parse("invalid conn.log duration (field 2)");
  }

  // Originator address
  i = fs.start(2);
  j = fs.end(2);
  e.emplace_back(ze::value::parse(ze::address_type, i, j));
  if (i != j)
    throw error::parse("invalid conn.log originating address (field 3)");

  // Responder address
  i = fs.start(3);
  j = fs.end(3);
  e.emplace_back(ze::value::parse(ze::address_type, i, j));
  if (i != j)
    throw error::parse("invalid conn.log responding address (field 4)");

  // Service
  i = fs.start(4);
  j = fs.end(4);
  if (*i == '?')
  {
    e.emplace_back(ze::nil);
  }
  else
  {
    e.emplace_back(ze::value::parse(ze::string_type, i, j));
    if (i != j)
      throw error::parse("invalid conn.log service (field 5)");
  }

  // Ports and protocol
  i = fs.start(5);
  j = fs.end(5);
  auto orig_p = ze::value::parse(ze::port_type, i, j);
  if (i != j)
    throw error::parse("invalid conn.log originating port (field 6)");

  i = fs.start(6);
  j = fs.end(6);
  auto resp_p = ze::value::parse(ze::port_type, i, j);
  if (i != j)
    throw error::parse("invalid conn.log responding port (field 7)");

  i = fs.start(7);
  j = fs.end(7);
  auto proto = ze::value::parse(ze::string_type, i, j);
  if (i != j)
    throw error::parse("invalid conn.log proto (field 8)");

  auto& str = proto.get<ze::string>();
  auto p = ze::port::unknown;
  if (str == "tcp")
    p = ze::port::tcp;
  else if (str == "udp")
    p = ze::port::udp;
  else if (str == "icmp")
    p = ze::port::icmp;
  orig_p.get<ze::port>().type(p);
  resp_p.get<ze::port>().type(p);
  e.emplace_back(std::move(orig_p));
  e.emplace_back(std::move(resp_p));
  e.emplace_back(std::move(proto));

  // Originator and responder bytes
  i = fs.start(8);
  j = fs.end(8);
  if (*i == '?')
  {
    e.emplace_back(ze::nil);
  }
  else
  {
    e.emplace_back(ze::value::parse(ze::uint_type, i, j));
    if (i != j)
      throw error::parse("invalid conn.log originating bytes (field 9)");
  }

  i = fs.start(8);
  j = fs.end(8);
  if (*i == '?')
  {
    e.emplace_back(ze::nil);
  }
  else
  {
    e.emplace_back(ze::value::parse(ze::uint_type, i, j));
    if (i != j)
      throw error::parse("invalid conn.log responding bytes (field 10)");
  }

  // Connection state
  i = fs.start(10);
  j = fs.end(10);
  e.emplace_back(ze::string::parse(i, j));
  if (i != j)
    throw error::parse("invalid conn.log connection state (field 11)");

  // Direction
  i = fs.start(11);
  j = fs.end(11);
  e.emplace_back(ze::string::parse(i, j));
  if (i != j)
    throw error::parse("invalid conn.log direction (field 12)");

  // Additional information
  if (fs.fields() == 13)
  {
    i = fs.start(12);
    j = fs.end(12);
    e.emplace_back(ze::string::parse(i, j));
    if (i != j)
      throw error::parse("invalid conn.log direction (field 12)");
  }

  return e;
}

} // namespace source
} // namespace vast