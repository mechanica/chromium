/*
 * Copyright © 1998-2004  David Turner and Werner Lemberg
 * Copyright © 2004,2007,2009,2010  Red Hat, Inc.
 * Copyright © 2011  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Owen Taylor, Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#include "hb-buffer-private.hh"

#include <string.h>



#ifndef HB_DEBUG_BUFFER
#define HB_DEBUG_BUFFER (HB_DEBUG+0)
#endif

/* Here is how the buffer works internally:
 *
 * There are two info pointers: info and out_info.  They always have
 * the same allocated size, but different lengths.
 *
 * As an optimization, both info and out_info may point to the
 * same piece of memory, which is owned by info.  This remains the
 * case as long as out_len doesn't exceed i at any time.
 * In that case, swap_buffers() is no-op and the glyph operations operate
 * mostly in-place.
 *
 * As soon as out_info gets longer than info, out_info is moved over
 * to an alternate buffer (which we reuse the pos buffer for!), and its
 * current contents (out_len entries) are copied to the new place.
 * This should all remain transparent to the user.  swap_buffers() then
 * switches info and out_info.
 */



/* Internal API */

bool
hb_buffer_t::enlarge (unsigned int size)
{
  if (unlikely (in_error))
    return false;

  unsigned int new_allocated = allocated;
  hb_glyph_position_t *new_pos = NULL;
  hb_glyph_info_t *new_info = NULL;
  bool separate_out = out_info != info;

  if (unlikely (_hb_unsigned_int_mul_overflows (size, sizeof (info[0]))))
    goto done;

  while (size > new_allocated)
    new_allocated += (new_allocated >> 1) + 32;

  ASSERT_STATIC (sizeof (info[0]) == sizeof (pos[0]));
  if (unlikely (_hb_unsigned_int_mul_overflows (new_allocated, sizeof (info[0]))))
    goto done;

  new_pos = (hb_glyph_position_t *) realloc (pos, new_allocated * sizeof (pos[0]));
  new_info = (hb_glyph_info_t *) realloc (info, new_allocated * sizeof (info[0]));

done:
  if (unlikely (!new_pos || !new_info))
    in_error = true;

  if (likely (new_pos))
    pos = new_pos;

  if (likely (new_info))
    info = new_info;

  out_info = separate_out ? (hb_glyph_info_t *) pos : info;
  if (likely (!in_error))
    allocated = new_allocated;

  return likely (!in_error);
}

bool
hb_buffer_t::make_room_for (unsigned int num_in,
			    unsigned int num_out)
{
  if (unlikely (!ensure (out_len + num_out))) return false;

  if (out_info == info &&
      out_len + num_out > idx + num_in)
  {
    assert (have_output);

    out_info = (hb_glyph_info_t *) pos;
    memcpy (out_info, info, out_len * sizeof (out_info[0]));
  }

  return true;
}

void *
hb_buffer_t::get_scratch_buffer (unsigned int *size)
{
  have_output = false;
  have_positions = false;

  out_len = 0;
  out_info = info;

  *size = allocated * sizeof (pos[0]);
  return pos;
}



/* HarfBuzz-Internal API */

void
hb_buffer_t::reset (void)
{
  if (unlikely (hb_object_is_inert (this)))
    return;

  hb_unicode_funcs_destroy (unicode);
  unicode = hb_unicode_funcs_get_default ();

  hb_segment_properties_t default_props = _HB_BUFFER_PROPS_DEFAULT;
  props = default_props;

  content_type = HB_BUFFER_CONTENT_TYPE_INVALID;
  in_error = false;
  have_output = false;
  have_positions = false;

  idx = 0;
  len = 0;
  out_len = 0;
  out_info = info;

  serial = 0;
  memset (allocated_var_bytes, 0, sizeof allocated_var_bytes);
  memset (allocated_var_owner, 0, sizeof allocated_var_owner);
}

void
hb_buffer_t::add (hb_codepoint_t  codepoint,
		  hb_mask_t       mask,
		  unsigned int    cluster)
{
  hb_glyph_info_t *glyph;

  if (unlikely (!ensure (len + 1))) return;

  glyph = &info[len];

  memset (glyph, 0, sizeof (*glyph));
  glyph->codepoint = codepoint;
  glyph->mask = mask;
  glyph->cluster = cluster;

  len++;
}

void
hb_buffer_t::clear_output (void)
{
  if (unlikely (hb_object_is_inert (this)))
    return;

  have_output = true;
  have_positions = false;

  out_len = 0;
  out_info = info;
}

void
hb_buffer_t::clear_positions (void)
{
  if (unlikely (hb_object_is_inert (this)))
    return;

  have_output = false;
  have_positions = true;

  out_len = 0;
  out_info = info;

  memset (pos, 0, sizeof (pos[0]) * len);
}

void
hb_buffer_t::swap_buffers (void)
{
  if (unlikely (in_error)) return;

  assert (have_output);
  have_output = false;

  if (out_info != info)
  {
    hb_glyph_info_t *tmp_string;
    tmp_string = info;
    info = out_info;
    out_info = tmp_string;
    pos = (hb_glyph_position_t *) out_info;
  }

  unsigned int tmp;
  tmp = len;
  len = out_len;
  out_len = tmp;

  idx = 0;
}


void
hb_buffer_t::replace_glyphs (unsigned int num_in,
			     unsigned int num_out,
			     const uint32_t *glyph_data)
{
  if (unlikely (!make_room_for (num_in, num_out))) return;

  merge_clusters (idx, idx + num_in);

  hb_glyph_info_t orig_info = info[idx];
  hb_glyph_info_t *pinfo = &out_info[out_len];
  for (unsigned int i = 0; i < num_out; i++)
  {
    *pinfo = orig_info;
    pinfo->codepoint = glyph_data[i];
    pinfo++;
  }

  idx  += num_in;
  out_len += num_out;
}

void
hb_buffer_t::output_glyph (hb_codepoint_t glyph_index)
{
  if (unlikely (!make_room_for (0, 1))) return;

  out_info[out_len] = info[idx];
  out_info[out_len].codepoint = glyph_index;

  out_len++;
}

void
hb_buffer_t::output_info (hb_glyph_info_t &glyph_info)
{
  if (unlikely (!make_room_for (0, 1))) return;

  out_info[out_len] = glyph_info;

  out_len++;
}

void
hb_buffer_t::copy_glyph (void)
{
  if (unlikely (!make_room_for (0, 1))) return;

  out_info[out_len] = info[idx];

  out_len++;
}

void
hb_buffer_t::replace_glyph (hb_codepoint_t glyph_index)
{
  if (unlikely (out_info != info || out_len != idx)) {
    if (unlikely (!make_room_for (1, 1))) return;
    out_info[out_len] = info[idx];
  }
  out_info[out_len].codepoint = glyph_index;

  idx++;
  out_len++;
}


void
hb_buffer_t::set_masks (hb_mask_t    value,
			hb_mask_t    mask,
			unsigned int cluster_start,
			unsigned int cluster_end)
{
  hb_mask_t not_mask = ~mask;
  value &= mask;

  if (!mask)
    return;

  if (cluster_start == 0 && cluster_end == (unsigned int)-1) {
    unsigned int count = len;
    for (unsigned int i = 0; i < count; i++)
      info[i].mask = (info[i].mask & not_mask) | value;
    return;
  }

  unsigned int count = len;
  for (unsigned int i = 0; i < count; i++)
    if (cluster_start <= info[i].cluster && info[i].cluster < cluster_end)
      info[i].mask = (info[i].mask & not_mask) | value;
}

void
hb_buffer_t::reverse_range (unsigned int start,
			    unsigned int end)
{
  unsigned int i, j;

  if (start == end - 1)
    return;

  for (i = start, j = end - 1; i < j; i++, j--) {
    hb_glyph_info_t t;

    t = info[i];
    info[i] = info[j];
    info[j] = t;
  }

  if (pos) {
    for (i = start, j = end - 1; i < j; i++, j--) {
      hb_glyph_position_t t;

      t = pos[i];
      pos[i] = pos[j];
      pos[j] = t;
    }
  }
}

void
hb_buffer_t::reverse (void)
{
  if (unlikely (!len))
    return;

  reverse_range (0, len);
}

void
hb_buffer_t::reverse_clusters (void)
{
  unsigned int i, start, count, last_cluster;

  if (unlikely (!len))
    return;

  reverse ();

  count = len;
  start = 0;
  last_cluster = info[0].cluster;
  for (i = 1; i < count; i++) {
    if (last_cluster != info[i].cluster) {
      reverse_range (start, i);
      start = i;
      last_cluster = info[i].cluster;
    }
  }
  reverse_range (start, i);
}

void
hb_buffer_t::merge_clusters (unsigned int start,
			     unsigned int end)
{
  if (unlikely (end - start < 2))
    return;

  unsigned int cluster = info[start].cluster;

  for (unsigned int i = start + 1; i < end; i++)
    cluster = MIN (cluster, info[i].cluster);

  /* Extend end */
  while (end < len && info[end - 1].cluster == info[end].cluster)
    end++;

  /* Extend start */
  while (idx < start && info[start - 1].cluster == info[start].cluster)
    start--;

  /* If we hit the start of buffer, continue in out-buffer. */
  if (idx == start)
    for (unsigned i = out_len; i && out_info[i - 1].cluster == info[start].cluster; i--)
      out_info[i - 1].cluster = cluster;

  for (unsigned int i = start; i < end; i++)
    info[i].cluster = cluster;
}
void
hb_buffer_t::merge_out_clusters (unsigned int start,
				 unsigned int end)
{
  if (unlikely (end - start < 2))
    return;

  unsigned int cluster = out_info[start].cluster;

  for (unsigned int i = start + 1; i < end; i++)
    cluster = MIN (cluster, out_info[i].cluster);

  /* Extend start */
  while (start && out_info[start - 1].cluster == out_info[start].cluster)
    start--;

  /* Extend end */
  while (end < out_len && out_info[end - 1].cluster == out_info[end].cluster)
    end++;

  /* If we hit the end of out-buffer, continue in buffer. */
  if (end == out_len)
    for (unsigned i = idx; i < len && info[i].cluster == out_info[end - 1].cluster; i++)
      info[i].cluster = cluster;

  for (unsigned int i = start; i < end; i++)
    out_info[i].cluster = cluster;
}

void
hb_buffer_t::guess_properties (void)
{
  if (unlikely (!len)) return;
  assert (content_type == HB_BUFFER_CONTENT_TYPE_UNICODE);

  /* If script is set to INVALID, guess from buffer contents */
  if (props.script == HB_SCRIPT_INVALID) {
    for (unsigned int i = 0; i < len; i++) {
      hb_script_t script = unicode->script (info[i].codepoint);
      if (likely (script != HB_SCRIPT_COMMON &&
		  script != HB_SCRIPT_INHERITED &&
		  script != HB_SCRIPT_UNKNOWN)) {
        props.script = script;
        break;
      }
    }
  }

  /* If direction is set to INVALID, guess from script */
  if (props.direction == HB_DIRECTION_INVALID) {
    props.direction = hb_script_get_horizontal_direction (props.script);
  }

  /* If language is not set, use default language from locale */
  if (props.language == HB_LANGUAGE_INVALID) {
    /* TODO get_default_for_script? using $LANGUAGE */
    props.language = hb_language_get_default ();
  }
}


static inline void
dump_var_allocation (const hb_buffer_t *buffer)
{
  char buf[80];
  for (unsigned int i = 0; i < 8; i++)
    buf[i] = '0' + buffer->allocated_var_bytes[7 - i];
  buf[8] = '\0';
  DEBUG_MSG (BUFFER, buffer,
	     "Current var allocation: %s",
	     buf);
}

void hb_buffer_t::allocate_var (unsigned int byte_i, unsigned int count, const char *owner)
{
  assert (byte_i < 8 && byte_i + count <= 8);

  if (DEBUG (BUFFER))
    dump_var_allocation (this);
  DEBUG_MSG (BUFFER, this,
	     "Allocating var bytes %d..%d for %s",
	     byte_i, byte_i + count - 1, owner);

  for (unsigned int i = byte_i; i < byte_i + count; i++) {
    assert (!allocated_var_bytes[i]);
    allocated_var_bytes[i]++;
    allocated_var_owner[i] = owner;
  }
}

void hb_buffer_t::deallocate_var (unsigned int byte_i, unsigned int count, const char *owner)
{
  if (DEBUG (BUFFER))
    dump_var_allocation (this);

  DEBUG_MSG (BUFFER, this,
	     "Deallocating var bytes %d..%d for %s",
	     byte_i, byte_i + count - 1, owner);

  assert (byte_i < 8 && byte_i + count <= 8);
  for (unsigned int i = byte_i; i < byte_i + count; i++) {
    assert (allocated_var_bytes[i]);
    assert (0 == strcmp (allocated_var_owner[i], owner));
    allocated_var_bytes[i]--;
  }
}

void hb_buffer_t::assert_var (unsigned int byte_i, unsigned int count, const char *owner)
{
  if (DEBUG (BUFFER))
    dump_var_allocation (this);

  DEBUG_MSG (BUFFER, this,
	     "Asserting var bytes %d..%d for %s",
	     byte_i, byte_i + count - 1, owner);

  assert (byte_i < 8 && byte_i + count <= 8);
  for (unsigned int i = byte_i; i < byte_i + count; i++) {
    assert (allocated_var_bytes[i]);
    assert (0 == strcmp (allocated_var_owner[i], owner));
  }
}

void hb_buffer_t::deallocate_var_all (void)
{
  memset (allocated_var_bytes, 0, sizeof (allocated_var_bytes));
  memset (allocated_var_owner, 0, sizeof (allocated_var_owner));
}

/* Public API */

hb_buffer_t *
hb_buffer_create ()
{
  hb_buffer_t *buffer;

  if (!(buffer = hb_object_create<hb_buffer_t> ()))
    return hb_buffer_get_empty ();

  buffer->reset ();

  return buffer;
}

hb_buffer_t *
hb_buffer_get_empty (void)
{
  static const hb_buffer_t _hb_buffer_nil = {
    HB_OBJECT_HEADER_STATIC,

    const_cast<hb_unicode_funcs_t *> (&_hb_unicode_funcs_nil),
    _HB_BUFFER_PROPS_DEFAULT,

    HB_BUFFER_CONTENT_TYPE_INVALID,
    true, /* in_error */
    true, /* have_output */
    true  /* have_positions */
  };

  return const_cast<hb_buffer_t *> (&_hb_buffer_nil);
}

hb_buffer_t *
hb_buffer_reference (hb_buffer_t *buffer)
{
  return hb_object_reference (buffer);
}

void
hb_buffer_destroy (hb_buffer_t *buffer)
{
  if (!hb_object_destroy (buffer)) return;

  hb_unicode_funcs_destroy (buffer->unicode);

  free (buffer->info);
  free (buffer->pos);

  free (buffer);
}

hb_bool_t
hb_buffer_set_user_data (hb_buffer_t        *buffer,
			 hb_user_data_key_t *key,
			 void *              data,
			 hb_destroy_func_t   destroy,
			 hb_bool_t           replace)
{
  return hb_object_set_user_data (buffer, key, data, destroy, replace);
}

void *
hb_buffer_get_user_data (hb_buffer_t        *buffer,
			 hb_user_data_key_t *key)
{
  return hb_object_get_user_data (buffer, key);
}


void
hb_buffer_set_content_type (hb_buffer_t              *buffer,
			    hb_buffer_content_type_t  content_type)
{
  buffer->content_type = content_type;
}

hb_buffer_content_type_t
hb_buffer_get_content_type (hb_buffer_t *buffer)
{
  return buffer->content_type;
}


void
hb_buffer_set_unicode_funcs (hb_buffer_t        *buffer,
			     hb_unicode_funcs_t *unicode)
{
  if (unlikely (hb_object_is_inert (buffer)))
    return;

  if (!unicode)
    unicode = hb_unicode_funcs_get_default ();


  hb_unicode_funcs_reference (unicode);
  hb_unicode_funcs_destroy (buffer->unicode);
  buffer->unicode = unicode;
}

hb_unicode_funcs_t *
hb_buffer_get_unicode_funcs (hb_buffer_t        *buffer)
{
  return buffer->unicode;
}

void
hb_buffer_set_direction (hb_buffer_t    *buffer,
			 hb_direction_t  direction)

{
  if (unlikely (hb_object_is_inert (buffer)))
    return;

  buffer->props.direction = direction;
}

hb_direction_t
hb_buffer_get_direction (hb_buffer_t    *buffer)
{
  return buffer->props.direction;
}

void
hb_buffer_set_script (hb_buffer_t *buffer,
		      hb_script_t  script)
{
  if (unlikely (hb_object_is_inert (buffer)))
    return;

  buffer->props.script = script;
}

hb_script_t
hb_buffer_get_script (hb_buffer_t *buffer)
{
  return buffer->props.script;
}

void
hb_buffer_set_language (hb_buffer_t   *buffer,
			hb_language_t  language)
{
  if (unlikely (hb_object_is_inert (buffer)))
    return;

  buffer->props.language = language;
}

hb_language_t
hb_buffer_get_language (hb_buffer_t *buffer)
{
  return buffer->props.language;
}


void
hb_buffer_reset (hb_buffer_t *buffer)
{
  buffer->reset ();
}

hb_bool_t
hb_buffer_pre_allocate (hb_buffer_t *buffer, unsigned int size)
{
  return buffer->ensure (size);
}

hb_bool_t
hb_buffer_allocation_successful (hb_buffer_t  *buffer)
{
  return !buffer->in_error;
}

void
hb_buffer_add (hb_buffer_t    *buffer,
	       hb_codepoint_t  codepoint,
	       hb_mask_t       mask,
	       unsigned int    cluster)
{
  buffer->add (codepoint, mask, cluster);
}

hb_bool_t
hb_buffer_set_length (hb_buffer_t  *buffer,
		      unsigned int  length)
{
  if (unlikely (hb_object_is_inert (buffer)))
    return length == 0;

  if (!buffer->ensure (length))
    return false;

  /* Wipe the new space */
  if (length > buffer->len) {
    memset (buffer->info + buffer->len, 0, sizeof (buffer->info[0]) * (length - buffer->len));
    if (buffer->have_positions)
      memset (buffer->pos + buffer->len, 0, sizeof (buffer->pos[0]) * (length - buffer->len));
  }

  buffer->len = length;
  return true;
}

unsigned int
hb_buffer_get_length (hb_buffer_t *buffer)
{
  return buffer->len;
}

/* Return value valid as long as buffer not modified */
hb_glyph_info_t *
hb_buffer_get_glyph_infos (hb_buffer_t  *buffer,
                           unsigned int *length)
{
  if (length)
    *length = buffer->len;

  return (hb_glyph_info_t *) buffer->info;
}

/* Return value valid as long as buffer not modified */
hb_glyph_position_t *
hb_buffer_get_glyph_positions (hb_buffer_t  *buffer,
                               unsigned int *length)
{
  if (!buffer->have_positions)
    buffer->clear_positions ();

  if (length)
    *length = buffer->len;

  return (hb_glyph_position_t *) buffer->pos;
}

void
hb_buffer_reverse (hb_buffer_t *buffer)
{
  buffer->reverse ();
}

void
hb_buffer_reverse_clusters (hb_buffer_t *buffer)
{
  buffer->reverse_clusters ();
}

void
hb_buffer_guess_properties (hb_buffer_t *buffer)
{
  buffer->guess_properties ();
}

#define ADD_UTF(T) \
	HB_STMT_START { \
	  if (text_length == -1) { \
	    text_length = 0; \
	    const T *p = (const T *) text; \
	    while (*p) { \
	      text_length++; \
	      p++; \
	    } \
	  } \
	  if (item_length == -1) \
	    item_length = text_length - item_offset; \
	  buffer->ensure (buffer->len + item_length * sizeof (T) / 4); \
	  const T *next = (const T *) text + item_offset; \
	  const T *end = next + item_length; \
	  while (next < end) { \
	    hb_codepoint_t u; \
	    const T *old_next = next; \
	    next = UTF_NEXT (next, end, u); \
	    hb_buffer_add (buffer, u, 1,  old_next - (const T *) text); \
	  } \
	} HB_STMT_END


#define UTF8_COMPUTE(Char, Mask, Len) \
  if (Char < 128) { Len = 1; Mask = 0x7f; } \
  else if ((Char & 0xe0) == 0xc0) { Len = 2; Mask = 0x1f; } \
  else if ((Char & 0xf0) == 0xe0) { Len = 3; Mask = 0x0f; } \
  else if ((Char & 0xf8) == 0xf0) { Len = 4; Mask = 0x07; } \
  else Len = 0;

static inline const uint8_t *
hb_utf8_next (const uint8_t *text,
	      const uint8_t *end,
	      hb_codepoint_t *unicode)
{
  uint8_t c = *text;
  unsigned int mask, len;

  /* TODO check for overlong sequences? */

  UTF8_COMPUTE (c, mask, len);
  if (unlikely (!len || (unsigned int) (end - text) < len)) {
    *unicode = -1;
    return text + 1;
  } else {
    hb_codepoint_t result;
    unsigned int i;
    result = c & mask;
    for (i = 1; i < len; i++)
      {
	if (unlikely ((text[i] & 0xc0) != 0x80))
	  {
	    *unicode = -1;
	    return text + 1;
	  }
	result <<= 6;
	result |= (text[i] & 0x3f);
      }
    *unicode = result;
    return text + len;
  }
}

void
hb_buffer_add_utf8 (hb_buffer_t  *buffer,
		    const char   *text,
		    int           text_length,
		    unsigned int  item_offset,
		    int           item_length)
{
  assert (buffer->content_type == HB_BUFFER_CONTENT_TYPE_UNICODE ||
	  (!buffer->len && buffer->content_type == HB_BUFFER_CONTENT_TYPE_INVALID));
  if (unlikely (hb_object_is_inert (buffer)))
    return;
  buffer->content_type = HB_BUFFER_CONTENT_TYPE_UNICODE;
#define UTF_NEXT(S, E, U)	hb_utf8_next (S, E, &(U))
  ADD_UTF (uint8_t);
#undef UTF_NEXT
}

static inline const uint16_t *
hb_utf16_next (const uint16_t *text,
	       const uint16_t *end,
	       hb_codepoint_t *unicode)
{
  uint16_t c = *text++;

  if (unlikely (c >= 0xd800 && c < 0xdc00)) {
    /* high surrogate */
    uint16_t l;
    if (text < end && ((l = *text), likely (l >= 0xdc00 && l < 0xe000))) {
      /* low surrogate */
      *unicode = ((hb_codepoint_t) ((c) - 0xd800) * 0x400 + (l) - 0xdc00 + 0x10000);
       text++;
    } else
      *unicode = -1;
  } else
    *unicode = c;

  return text;
}

void
hb_buffer_add_utf16 (hb_buffer_t    *buffer,
		     const uint16_t *text,
		     int             text_length,
		     unsigned int    item_offset,
		     int            item_length)
{
  assert (buffer->content_type == HB_BUFFER_CONTENT_TYPE_UNICODE ||
	  (!buffer->len && buffer->content_type == HB_BUFFER_CONTENT_TYPE_INVALID));
  if (unlikely (hb_object_is_inert (buffer)))
    return;
  buffer->content_type = HB_BUFFER_CONTENT_TYPE_UNICODE;
#define UTF_NEXT(S, E, U)	hb_utf16_next (S, E, &(U))
  ADD_UTF (uint16_t);
#undef UTF_NEXT
}

void
hb_buffer_add_utf32 (hb_buffer_t    *buffer,
		     const uint32_t *text,
		     int             text_length,
		     unsigned int    item_offset,
		     int             item_length)
{
  assert (buffer->content_type == HB_BUFFER_CONTENT_TYPE_UNICODE ||
	  (!buffer->len && buffer->content_type == HB_BUFFER_CONTENT_TYPE_INVALID));
  if (unlikely (hb_object_is_inert (buffer)))
    return;
  buffer->content_type = HB_BUFFER_CONTENT_TYPE_UNICODE;
#define UTF_NEXT(S, E, U)	((U) = *(S), (S)+1)
  ADD_UTF (uint32_t);
#undef UTF_NEXT
}


static int
compare_info_codepoint (const hb_glyph_info_t *pa,
			const hb_glyph_info_t *pb)
{
  return (int) pb->codepoint - (int) pa->codepoint;
}

static inline void
normalize_glyphs_cluster (hb_buffer_t *buffer,
			  unsigned int start,
			  unsigned int end,
			  bool backward)
{
  hb_glyph_position_t *pos = buffer->pos;

  /* Total cluster advance */
  hb_position_t total_x_advance = 0, total_y_advance = 0;
  for (unsigned int i = start; i < end; i++)
  {
    total_x_advance += pos[i].x_advance;
    total_y_advance += pos[i].y_advance;
  }

  hb_position_t x_advance = 0, y_advance = 0;
  for (unsigned int i = start; i < end; i++)
  {
    pos[i].x_offset += x_advance;
    pos[i].y_offset += y_advance;

    x_advance += pos[i].x_advance;
    y_advance += pos[i].y_advance;

    pos[i].x_advance = 0;
    pos[i].y_advance = 0;
  }

  if (backward)
  {
    /* Transfer all cluster advance to the last glyph. */
    pos[end - 1].x_advance = total_x_advance;
    pos[end - 1].y_advance = total_y_advance;

    hb_bubble_sort (buffer->info + start, end - start - 1, compare_info_codepoint, buffer->pos + start);
  } else {
    /* Transfer all cluster advance to the first glyph. */
    pos[start].x_advance += total_x_advance;
    pos[start].y_advance += total_y_advance;
    for (unsigned int i = start + 1; i < end; i++) {
      pos[i].x_offset -= total_x_advance;
      pos[i].y_offset -= total_y_advance;
    }
    hb_bubble_sort (buffer->info + start + 1, end - start - 1, compare_info_codepoint, buffer->pos + start + 1);
  }
}

void
hb_buffer_normalize_glyphs (hb_buffer_t *buffer)
{
  assert (buffer->have_positions);
  /* XXX assert (buffer->have_glyphs); */

  bool backward = HB_DIRECTION_IS_BACKWARD (buffer->props.direction);

  unsigned int count = buffer->len;
  if (unlikely (!count)) return;
  hb_glyph_info_t *info = buffer->info;

  unsigned int start = 0;
  unsigned int end;
  for (end = start + 1; end < count; end++)
    if (info[start].cluster != info[end].cluster) {
      normalize_glyphs_cluster (buffer, start, end, backward);
      start = end;
    }
  normalize_glyphs_cluster (buffer, start, end, backward);
}
