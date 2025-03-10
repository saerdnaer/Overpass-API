/** Copyright 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018 Roland Olbricht et al.
 *
 * This file is part of Overpass_API.
 *
 * Overpass_API is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Overpass_API is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Overpass_API.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DE__OSM3S___TEMPLATE_DB__FILE_BLOCKS_INDEX_H
#define DE__OSM3S___TEMPLATE_DB__FILE_BLOCKS_INDEX_H

#include "types.h"

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <list>
#include <string>
#include <vector>

/** Declarations: -----------------------------------------------------------*/


template< class TIndex >
struct File_Block_Index_Entry
{
  static const int EMPTY = 1;
  static const int GROUP = 2;
  static const int SEGMENT = 3;
  static const int LAST_SEGMENT = 4;

  File_Block_Index_Entry(const TIndex& index_, uint32 pos_, uint32 size_, uint32 max_keysize_)
    : index(index_), pos(pos_), size(size_), max_keysize(max_keysize_) {}

  TIndex index;
  uint32 pos;
  uint32 size;
  uint32 max_keysize;
};


template< class TIndex >
struct File_Blocks_Index : public File_Blocks_Index_Base
{
public:
  File_Blocks_Index(const File_Properties& file_prop,
	      bool writeable, bool use_shadow,
	      const std::string& db_dir, const std::string& file_name_extension,
              int compression_method_ = USE_DEFAULT);
  virtual ~File_Blocks_Index();
  bool writeable() const { return (empty_index_file_name != ""); }
  const std::string& file_name_extension() const { return file_name_extension_; }

  std::string get_data_file_name() const { return data_file_name; }
  uint64 get_block_size() const { return block_size_; }
  uint32 get_compression_factor() const { return compression_factor; }
  uint32 get_compression_method() const { return compression_method; }
  virtual bool empty() const { return file_size == 0; }

  std::list< File_Block_Index_Entry< TIndex > >& get_block_list()
  {
    if (index_buf.ptr)
      init_blocks();
    if (block_list.empty() && !block_array.empty())
      block_list.assign(block_array.begin(), block_array.end());
    return block_list;
  }
  const std::vector< File_Block_Index_Entry< TIndex > >& get_blocks()
  {
    if (index_buf.ptr)
      init_blocks();
    if (block_array.empty() && !block_list.empty())
      block_array.assign(block_list.begin(), block_list.end());
    return block_array;
  }
  std::vector< std::pair< uint32, uint32 > >& get_void_blocks()
  {
    if (!void_blocks_initialized)
      init_void_blocks();
    return void_blocks;
  }
  void drop_block_array()
  {
    if (block_list.empty() && !block_array.empty())
      block_list.assign(block_array.begin(), block_array.end());
    block_array.clear();
  }

  static const int FILE_FORMAT_VERSION = 7560;
  static const int NO_COMPRESSION = 0;
  static const int ZLIB_COMPRESSION = 1;
  static const int LZ4_COMPRESSION = 2;

private:
  std::string index_file_name;
  std::string empty_index_file_name;
  std::string data_file_name;
  std::string file_name_extension_;
  Void_Pointer< uint8 > index_buf;
  uint64 file_size;
  uint32 index_size;
  std::vector< File_Block_Index_Entry< TIndex > > block_array;
  std::list< File_Block_Index_Entry< TIndex > > block_list;
  std::vector< std::pair< uint32, uint32 > > void_blocks;
  bool void_blocks_initialized;

  uint64 block_size_;
  uint32 compression_factor;
  int compression_method;

  void init_structure_params();
  void init_blocks();
  void init_void_blocks();

public:
  uint32 block_count;
};


template< class TIndex >
std::vector< bool > get_data_index_footprint(const File_Properties& file_prop,
					std::string db_dir);

/** Implementation File_Blocks_Index: ---------------------------------------*/

template< class TIndex >
File_Blocks_Index< TIndex >::File_Blocks_Index
    (const File_Properties& file_prop, bool writeable, bool use_shadow,
     const std::string& db_dir, const std::string& file_name_extension,
     int compression_method_) :
     index_file_name(db_dir + file_prop.get_file_name_trunk()
         + file_name_extension + file_prop.get_data_suffix()
         + file_prop.get_index_suffix()
	 + (use_shadow ? file_prop.get_shadow_suffix() : "")),
     empty_index_file_name(writeable ? db_dir + file_prop.get_file_name_trunk()
         + file_name_extension + file_prop.get_data_suffix()
         + file_prop.get_shadow_suffix() : ""),
     data_file_name(db_dir + file_prop.get_file_name_trunk()
         + file_name_extension + file_prop.get_data_suffix()),
     file_name_extension_(file_name_extension),
     index_buf(0), file_size(0), index_size(0),
     void_blocks_initialized(false),
     block_size_(file_prop.get_block_size()), // can be overwritten by index file
     compression_factor(file_prop.get_compression_factor()), // can be overwritten by index file
     compression_method(compression_method_ == USE_DEFAULT ?
        file_prop.get_compression_method() : compression_method_), // can be overwritten by index file
     block_count(0)
{
  try
  {
    Raw_File val_file(data_file_name, O_RDONLY, S_666, "File_Blocks_Index::File_Blocks_Index::1");
    file_size = val_file.size("File_Blocks_Index::File_Blocks_Index::2");
  }
  catch (File_Error e)
  {
    if (e.error_number != 2)
      throw e;
  }

  try
  {
    Raw_File source_file(index_file_name, O_RDONLY, S_666,
			 "File_Blocks_Index::File_Blocks_Index::3");

    // read index file
    index_size = source_file.size("File_Blocks_Index::File_Blocks_Index::4");
    index_buf.resize(index_size);
    source_file.read(index_buf.ptr, index_size, "File_Blocks_Index::File_Blocks_Index::5");
  }
  catch (File_Error e)
  {
    if (e.error_number != 2)
      throw e;
    index_buf.resize(0);
  }

  init_structure_params();

  if (empty_index_file_name != "")
    init_void_blocks();
}


template< class TIndex >
void File_Blocks_Index< TIndex >::init_structure_params()
{
  if (index_buf.ptr)
  {
    if (file_name_extension_ != ".legacy")
    {
      if (*(int32*)index_buf.ptr != FILE_FORMAT_VERSION && *(int32*)index_buf.ptr != 7512)
	throw File_Error(0, index_file_name, "File_Blocks_Index: Unsupported index file format version");
      block_size_ = 1ull<<*(uint8*)(index_buf.ptr + 4);
      if (!block_size_)
        throw File_Error(0, index_file_name, "File_Blocks_Index: Illegal block size");
      compression_factor = 1u<<*(uint8*)(index_buf.ptr + 5);
      if (!compression_factor || compression_factor > block_size_)
        throw File_Error(0, index_file_name, "File_Blocks_Index: Illegal compression factor");
      compression_method = *(uint16*)(index_buf.ptr + 6);
    }
    if (file_size % block_size_)
      throw File_Error(0, index_file_name, "File_Blocks_Index: Data file size does not match block size");
    block_count = file_size / block_size_;
  }
}


template< class TIndex >
void File_Blocks_Index< TIndex >::init_blocks()
{
  if (index_buf.ptr)
  {
    if (file_name_extension_ == ".legacy")
      // We support this way the old format although it has no version marker.
    {
      uint32 pos = 0;
      while (pos < index_size)
      {
        TIndex index(index_buf.ptr+pos);
        File_Block_Index_Entry< TIndex >
            entry(index,
	    *(uint32*)(index_buf.ptr + (pos + TIndex::size_of(index_buf.ptr+pos))),
	    1, //block size is always 1 in the legacy format
	    *(uint32*)(index_buf.ptr + (pos + TIndex::size_of(index_buf.ptr+pos) + 4)));
        if (writeable())
          block_list.push_back(entry);
        else
          block_array.push_back(entry);
        if (entry.pos >= block_count)
	  throw File_Error(0, index_file_name, "File_Blocks_Index: bad pos in index file");
        pos += TIndex::size_of(index_buf.ptr+pos) + 8;
      }
    }
    else if (index_size > 0)
    {
      uint32 pos = 8;
      while (pos < index_size)
      {
        TIndex index(index_buf.ptr + pos + 12);
        File_Block_Index_Entry< TIndex >
            entry(index,
	    *(uint32*)(index_buf.ptr + pos),
	    *(uint32*)(index_buf.ptr + pos + 4),
	    *(uint32*)(index_buf.ptr + pos + 8));
        if (writeable())
          block_list.push_back(entry);
        else
          block_array.push_back(entry);
        if (entry.pos >= block_count)
          throw File_Error(0, index_file_name, "File_Blocks_Index: bad pos in index file");
        if (entry.pos + entry.size > block_count)
          throw File_Error(0, index_file_name, "File_Blocks_Index: bad size in index file");
        pos += 12;
        pos += TIndex::size_of(index_buf.ptr + pos);
      }
    }

    index_buf.resize(0);
  }
}


template< class TIndex >
void File_Blocks_Index< TIndex >::init_void_blocks()
{
  if (index_buf.ptr)
    init_blocks();

  bool empty_index_file_used = false;
  if (empty_index_file_name != "")
  {
    try
    {
      Raw_File void_blocks_file(empty_index_file_name, O_RDONLY, S_666, "");
      uint32 void_index_size = void_blocks_file.size("File_Blocks_Index::File_Blocks_Index::6");
      Void_Pointer< uint8 > index_buf(void_index_size);
      void_blocks_file.read(index_buf.ptr, void_index_size,
			      "File_Blocks_Index::File_Blocks_Index::7");
      for (uint32 i = 0; i < void_index_size/8; ++i)
        void_blocks.push_back(*(std::pair< uint32, uint32 >*)(index_buf.ptr + 8*i));
      empty_index_file_used = true;
    }
    catch (File_Error e) {}
  }

  if (!empty_index_file_used)
  {
    std::vector< bool > is_referred(block_count, false);
    for (typename std::list< File_Block_Index_Entry< TIndex > >::const_iterator it = block_list.begin();
        it != block_list.end(); ++it)
    {
      for (uint32 i = 0; i < it->size; ++i)
        is_referred[it->pos + i] = true;
    }
    for (typename std::vector< File_Block_Index_Entry< TIndex > >::const_iterator it = block_array.begin();
        it != block_array.end(); ++it)
    {
      for (uint32 i = 0; i < it->size; ++i)
        is_referred[it->pos + i] = true;
    }

    // determine void_blocks
    uint32 last_start = 0;
    for (uint32 i = 0; i < block_count; ++i)
    {
      if (is_referred[i])
      {
        if (last_start < i)
          void_blocks.push_back(std::make_pair(i - last_start, last_start));
        last_start = i+1;
      }
    }
    if (last_start < block_count)
      void_blocks.push_back(std::make_pair(block_count - last_start, last_start));
  }

  std::stable_sort(void_blocks.begin(), void_blocks.end());
  void_blocks_initialized = true;
}


template< class TIndex >
File_Blocks_Index< TIndex >::~File_Blocks_Index()
{
  if (!writeable())
    return;

  // Keep space for file version and size information
  uint32 index_size = 8;
  uint32 pos = 8;

  for (typename std::list< File_Block_Index_Entry< TIndex > >::const_iterator
      it(block_list.begin()); it != block_list.end(); ++it)
    index_size += 12 + it->index.size_of();

  Void_Pointer< uint8 > index_buf(index_size);

  *(uint32*)index_buf.ptr = FILE_FORMAT_VERSION;
  *(uint8*)(index_buf.ptr + 4) = shift_log(block_size_);
  *(uint8*)(index_buf.ptr + 5) = shift_log(compression_factor);
  *(uint16*)(index_buf.ptr + 6) = compression_method;

  for (typename std::list< File_Block_Index_Entry< TIndex > >::const_iterator
      it(block_list.begin()); it != block_list.end(); ++it)
  {
    *(uint32*)(index_buf.ptr+pos) = it->pos;
    pos += 4;
    *(uint32*)(index_buf.ptr+pos) = it->size;
    pos += 4;
    *(uint32*)(index_buf.ptr+pos) = it->max_keysize;
    pos += 4;
    it->index.to_data(index_buf.ptr+pos);
    pos += it->index.size_of();
  }

  Raw_File dest_file(index_file_name, O_RDWR|O_CREAT, S_666,
		     "File_Blocks_Index::~File_Blocks_Index::1");

  if (index_size < dest_file.size("File_Blocks_Index::~File_Blocks_Index::2"))
    dest_file.resize(index_size, "File_Blocks_Index::~File_Blocks_Index::3");
  dest_file.write(index_buf.ptr, index_size, "File_Blocks_Index::~File_Blocks_Index::4");

  // Write void blocks
  Void_Pointer< uint8 > void_index_buf(void_blocks.size() * 8);
  std::pair< uint32, uint32 >* it_ptr = (std::pair< uint32, uint32 >*)(void_index_buf.ptr);
  for (std::vector< std::pair< uint32, uint32 > >::const_iterator it(void_blocks.begin());
      it != void_blocks.end(); ++it)
    *(it_ptr++) = *it;

  try
  {
    Raw_File void_file(empty_index_file_name, O_RDWR|O_TRUNC, S_666,
		       "File_Blocks_Index::~File_Blocks_Index::5");
    void_file.write(void_index_buf.ptr, void_blocks.size()*sizeof(uint32),
		    "File_Blocks_Index::~File_Blocks_Index::6");
  }
  catch (File_Error e) {}
}

/** Implementation non-members: ---------------------------------------------*/

template< class TIndex >
std::vector< bool > get_data_index_footprint
    (const File_Properties& file_prop, std::string db_dir)
{
  File_Blocks_Index< TIndex > index(file_prop, false, false, db_dir, "");

  std::vector< bool > result(index.block_count, true);
  for (typename std::vector< std::pair< uint32, uint32 > >::const_iterator
      it = index.get_void_blocks().begin(); it != index.get_void_blocks().end(); ++it)
  {
    for (uint32 i = 0; i < it->first; ++i)
      result[it->second + i] = false;
  }
  return result;
}

#endif
