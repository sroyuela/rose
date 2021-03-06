// tps (01/14/2010) : Switching from rose.h to sage3.
#include "sage3basic.h"
#include <errno.h>
#include <fcntl.h>
#ifndef _MSC_VER
#include <sys/mman.h>
#else
#include <io.h>
#endif
#include "rose_getline.h"
/* See header file for full documentation */

/************************************************************************************************************************
 * Output functions for MemoryMap exceptions
 ************************************************************************************************************************/

std::ostream&
operator<<(std::ostream &o, const MemoryMap::Exception &e)
{
    e.print(o);
    return o;
}

std::ostream&
operator<<(std::ostream &o, const MemoryMap::Inconsistent &e)
{
    e.print(o);
    return o;
}

std::ostream&
operator<<(std::ostream &o, const MemoryMap::NotMapped &e)
{
    e.print(o);
    return o;
}

std::ostream&
operator<<(std::ostream &o, const MemoryMap::NoFreeSpace &e)
{
    e.print(o);
    return o;
}

std::ostream&
operator<<(std::ostream &o, const MemoryMap::Syntax &e)
{
    e.print(o);
    return o;
}

void
MemoryMap::Exception::print(std::ostream &o) const
{
    o <<"error in memory map";
}

void
MemoryMap::Inconsistent::print(std::ostream &o) const
{
    using namespace StringUtility;
    o <<"inconsistent mapping between elements"
      <<" (" <<addrToString(a.get_va()) <<"+" <<addrToString(a.get_size()) <<"=" <<addrToString(a.get_va()+a.get_size()) <<")"
      <<" and"
      <<" (" <<addrToString(b.get_va()) <<"+" <<addrToString(b.get_size()) <<"=" <<addrToString(b.get_va()+b.get_size()) <<")";
}

void
MemoryMap::NotMapped::print(std::ostream &o) const
{
    o <<"address " <<StringUtility::addrToString(va) <<" is not present in the memory map";
}

void
MemoryMap::NoFreeSpace::print(std::ostream &o) const
{
    o <<"memory map does not have " <<StringUtility::addrToString(size) <<" bytes of free space";
}

void
MemoryMap::Syntax::print(std::ostream &o) const
{
    o <<(filename.empty()?"at ":(filename+":"));
    o <<linenum;
    if (colnum>=0)
        o <<"." <<colnum;
    o <<": " <<(mesg.empty()?"syntax error":mesg);
}

/************************************************************************************************************************
 * End of exception output functions
 ************************************************************************************************************************/


void *
MemoryMap::MapElement::get_base(bool allocate_anonymous/*=true*/) const
{
    if (anonymous && allocate_anonymous) {
        if (NULL==anonymous->base) {
            ROSE_ASSERT(NULL==base);
            base = anonymous->base = new uint8_t[get_size()];
            memset(anonymous->base, 0, get_size());
        } else {
            ROSE_ASSERT(base==anonymous->base);
        }
    }
    return base;
}

rose_addr_t
MemoryMap::MapElement::get_va_offset(rose_addr_t va, size_t nbytes) const
{
    if (va<get_va() || va+nbytes>get_va()+get_size())
        throw NotMapped(NULL, va);
    return get_offset() + (va - get_va());
}

bool
MemoryMap::MapElement::consistent(const MapElement &other) const
{
    if (is_read_only()!=other.is_read_only()) {
        return false;
    } else if (get_mapperms()!=other.get_mapperms()) {
        return false;
    } else if (is_anonymous() && other.is_anonymous()) {
        if (anonymous==other.anonymous)
            return true;
        if (NULL==get_base(false) && NULL==other.get_base(false))
            return true;
        return false;
    } else if (is_anonymous() || other.is_anonymous()) {
        return false;
    } else if (get_base()!=other.get_base()) { /*neither is anonymous*/
        return false;
    } else {
        return va - other.va == offset - other.offset;
    }
}

std::string
MemoryMap::MapElement::get_name_pairings(NamePairings *pairings) const
{
    ROSE_ASSERT(pairings!=NULL);
    std::string retval;
    std::string::size_type i = 0;
    while (i<name.size()) {
        /* Extract the file name up to the left paren */
        while (i<name.size() && isspace(name[i])) i++;
        std::string::size_type fname_start = i;
        while (i<name.size() && !isspace(name[i]) && !strchr("()+", name[i])) i++;
        if (i>=name.size() || '('!=name[i] || fname_start==i) {
            retval += name.substr(fname_start, i-fname_start);
            break;
        }
        std::string fname = name.substr(fname_start, i-fname_start);
        i++; /*skip over the left paren*/
        int parens=0;
        
        /* Extract name(s) separated from one another by '+' */
        while (i<name.size() && ')'!=name[i]) {
            while (i<name.size() && isspace(name[i])) i++;
            std::string::size_type gname_start = i;
            while (i<name.size() && '+'!=name[i] && (parens>0 || ')'!=name[i])) {
                if ('('==name[i]) {
                    parens++;
                } else if (')'==name[i]) {
                    parens--;
                } 
                i++;
            }
            if (i>gname_start)
                (*pairings)[fname].insert(name.substr(gname_start, i-gname_start));
            if (i<name.size() && '+'==name[i]) i++;
        }

        /* Skip over right paren and optional space and '+' */
        if (i<name.size() && ')'==name[i]) i++;
        while (i<name.size() && isspace(name[i])) i++;
        if (i<name.size() && '+'==name[i]) i++;
    }
    if (i<name.size())
        retval += name.substr(i);
    return retval;
}

MemoryMap::MapElement&
MemoryMap::MapElement::set_name(const NamePairings &pairings, const std::string &s1, const std::string &s2)
{
    std::string s;
    for (NamePairings::const_iterator pi=pairings.begin(); pi!=pairings.end(); ++pi) {
        s += (s.empty()?"":"+") + pi->first + "(";
        for (std::set<std::string>::const_iterator si=pi->second.begin(); si!=pi->second.end(); ++si) {
            if ('('!=s[s.size()-1]) s += "+";
            s += *si;
        }
        s += ')';
    }
    if (!s1.empty())
        s += (s.empty()?"":"+") + s1;
    if (!s2.empty())
        s += (s.empty()?"":"+") + s2;
    set_name(s);
    return *this;
}

MemoryMap::MapElement &
MemoryMap::MapElement::set_name(const std::string &s)
{
    name = s;
    return *this;
}

void
MemoryMap::MapElement::merge_names(const MapElement &other)
{
    if (name.empty()) {
        set_name(other.get_name());
    } else if (other.name.empty() || get_name()==other.get_name()) {
        /*void*/
    } else {
        NamePairings pairings;
        std::string s1 = get_name_pairings(&pairings);
        std::string s2 = other.get_name_pairings(&pairings);
        set_name(pairings, s1, s2);
    }
}

bool
MemoryMap::MapElement::merge(const MapElement &other)
{
    if (va+size < other.va || va > other.va+other.size) {
        /* Other element is left or right of this one and not contiguous with it. */
        return false;
    } else if (other.va >= va && other.va+other.size <= va+size) {
        /* Other element is contained within (or congruent to) this element. */
        if (!consistent(other))
            throw Inconsistent(NULL, *this, other);
    } else if (va >= other.va && va+size <= other.va+other.size) {
        /* Other element encloses this element. */
        if (!consistent(other))
            throw Inconsistent(NULL, *this, other);
        offset = other.offset;
        va = other.va;
        base = other.base;
        size = other.size;
        merge_names(other);
    } else if (other.va + other.size == va) {
        /* Other element is left contiguous with this element. */
        if (!consistent(other))
            return false; /*no exception since they don't overlap*/
        size += other.size;
        va = other.va;
        base = other.base;
        offset = other.offset;
        merge_names(other);
    } else if (va + size == other.va) {
        /* Other element is right contiguous with this element. */
        if (!consistent(other))
            return false; /*no exception since they don't overlap*/
        size += other.size;
        merge_names(other);
    } else if (other.va < va) {
        /* Other element overlaps left part of this element. */
        if (!consistent(other))
            throw Inconsistent(NULL, *this, other);
        size += va - other.va;
        va = other.va;
        base = other.base;
        offset = other.offset;
        merge_names(other);
    } else {
        /* Other element overlaps right part of this element. */
        if (!consistent(other))
            throw Inconsistent(NULL, *this, other);
        size = (other.va + other.size) - va;
        merge_names(other);
    }

    return true;
}

void
MemoryMap::clear()
{
    elements.clear();
}

void
MemoryMap::insert(MapElement add)
{
    if (add.size==0)
        return;

    try {
        /* Remove existing elements that are contiguous with or overlap with the new element, extending the new element to cover
         * the removed element. We also check the consistency of the mapping and throw an exception if the new element overlaps
         * inconsistently with an existing element. */
        std::vector<MapElement>::iterator i=elements.begin();
        while (i!=elements.end()) {
            MapElement &old = *i;
            if (add.merge(old)) {
                i = elements.erase(i);
            } else {
                ++i;
            }
        }

        /* Insert the new element */
        assert(NULL==find(add.va));
        elements.push_back(add);

        /* Keep elements sorted */
        std::sort(elements.begin(), elements.end());
    } catch (Exception &e) {
        e.map = this;
        throw e;
    }
}
    
void
MemoryMap::erase(MapElement me)
{
    if (me.size==0)
        return;

    try {
        /* Remove existing elements that overlap with the erasure area, reducing their size to the part that doesn't overlap, and
         * then add the non-overlapping parts back at the end. */
        std::vector<MapElement>::iterator i=elements.begin();
        std::vector<MapElement> saved;
        while (i!=elements.end()) {
            MapElement &old = *i;
            if (me.va+me.size <= old.va || old.va+old.size <= me.va) {
                /* Non overlapping */
                ++i;
                continue;
            }

            if (me.va > old.va) {
                /* Erasure begins right of existing element. */
                MapElement tosave = old;
                tosave.size = me.va-old.va;
                saved.push_back(tosave);
            }
            if (me.va+me.size < old.va+old.size) {
                /* Erasure ends left of existing element. */
                MapElement tosave = old;
                tosave.va = me.va+me.size;
                tosave.size = (old.va+old.size) - (me.va+me.size);
                tosave.offset += (me.va+me.size) - old.va;
                saved.push_back(tosave);
            }
            i = elements.erase(i);
        }

        /* Now add saved elements back in. */
        for (i=saved.begin(); i!=saved.end(); ++i)
            insert(*i);
    } catch(Exception &e) {
        e.map = this;
        throw e;
    }
}

const MemoryMap::MapElement *
MemoryMap::find(rose_addr_t va) const
{
    size_t lo=0, hi=elements.size();
    while (lo<hi) {
        size_t mid=(lo+hi)/2;
        const MapElement &elmt = elements[mid];
        if (va < elmt.va) {
            hi = mid;
        } else if (va >= elmt.va+elmt.size) {
            lo = mid+1;
        } else {
            return &elmt;
        }
    }
    return NULL;
}

rose_addr_t
MemoryMap::find_free(rose_addr_t start_va, size_t size, rose_addr_t alignment) const
{
    start_va = ALIGN_UP(start_va, alignment);
    for (size_t i=0; i<elements.size(); i++) {
        const MapElement &me = elements[i];
        if (me.va + me.size <= start_va)
            continue;
        if (me.va > start_va &&  me.va - start_va >= size)
            break;
        rose_addr_t x = start_va;
        start_va = ALIGN_UP(me.va + me.size, alignment);
        if (start_va<x)
            throw NoFreeSpace(this, size);
    }

    if (start_va+size < start_va)
        throw NoFreeSpace(this, size);

    return start_va;
}

rose_addr_t
MemoryMap::find_last_free(rose_addr_t max) const
{
    bool found = false;
    rose_addr_t retval = 0;

    /* Is the null address free? */
    if (elements.empty() || elements[0].get_va()>0) {
        retval = 0;
        found = true;
    }

    /* Try to find a higher free region */
    for (size_t i=0; i<elements.size(); i++) {
        rose_addr_t end = elements[i].get_va() + elements[i].get_size();
        if (end > max) break;
        if (i+1>=elements.size() || elements[i+1].get_va()>end) {
            retval = end;
            found = true;
        }
    }
    
    if (!found)
        throw NoFreeSpace(this, 1);
    return retval;
}

const std::vector<MemoryMap::MapElement> &
MemoryMap::get_elements() const {
    return elements;
}

void
MemoryMap::prune(bool(*predicate)(const MapElement&))
{
    std::vector<MapElement> keep;
    for (size_t i=0; i<elements.size(); i++) {
        if (!predicate(elements[i]))
            keep.push_back(elements[i]);
    }
    elements = keep;
}

void
MemoryMap::prune(unsigned required, unsigned prohibited)
{
    std::vector<MapElement> keep;
    for (size_t i=0; i<elements.size(); ++i) {
        if ((0==required || 0!=(elements[i].get_mapperms() & required)) &&
            0==(elements[i].get_mapperms() & prohibited))
            keep.push_back(elements[i]);
    }
    elements = keep;
}

size_t
MemoryMap::read1(void *dst_buf, rose_addr_t va, size_t desired, unsigned req_perms/*=MM_PROT_READ*/,
                 const MapElement **mep/*=NULL*/) const
{
    const MemoryMap::MapElement *m = find(va);
    if (mep) *mep = m;
    if (!m || (m->get_mapperms() & req_perms)!=req_perms)
        return 0;
    ROSE_ASSERT(va >= m->get_va());
    size_t m_offset = va - m->get_va();
    ROSE_ASSERT(m_offset < m->get_size());
    size_t n = std::min(desired, m->get_size()-m_offset);

    /* If there have been no writes to an anonymous element and no base has been allocated, then just fill
     * the return value with zeros */
    if (dst_buf!=NULL) {
        if (m->is_anonymous() && NULL==m->get_base(false)) {
            memset(dst_buf, 0, n);
        } else {
            memcpy(dst_buf, (uint8_t*)m->get_base()+m->get_offset()+m_offset, n);
        }
    }
    return n;
}

size_t
MemoryMap::read(void *dst_buf, rose_addr_t start_va, size_t desired, unsigned req_perms/*=MM_PROT_READ*/) const
{
    size_t total_copied=0;
    while (total_copied < desired) {
        size_t n = read1((uint8_t*)dst_buf+total_copied, start_va+total_copied, desired-total_copied, req_perms, NULL);
        if (!n) break;
        total_copied += n;
    }

    memset((uint8_t*)dst_buf+total_copied, 0, desired-total_copied);
    return total_copied;
}

SgUnsignedCharList
MemoryMap::read(rose_addr_t va, size_t desired, unsigned req_perms/*=MM_PROT_READ*/) const
{
    SgUnsignedCharList retval;
    while (desired>0) {
        const MemoryMap::MapElement *m=NULL;
        size_t can_read = read1(NULL, va, desired, req_perms, &m);
        if (0==can_read)
            break;
        assert(m!=NULL && va>=m->get_va());
        size_t m_offset = va - m->get_va();
        assert(m_offset+can_read<=m->get_size());

        size_t retval_offset = retval.size();
        retval.resize(retval.size()+can_read, 0);
        if (!m->is_anonymous() || NULL!=m->get_base(false))
            memcpy(&retval[retval_offset], (uint8_t*)m->get_base()+m->get_offset()+m_offset, can_read);

        va += can_read;
        desired -= can_read;
    }
    return retval;
}

size_t
MemoryMap::write1(const void *src_buf, rose_addr_t va, size_t nbytes, unsigned req_perms/*=MM_PROT_WRITE*/,
                  const MapElement **mep/*=NULL*/) const
{
    const MemoryMap::MapElement *m = find(va);
    if (mep) *mep = m;
    if (!m || m->is_read_only() || (m->get_mapperms() & req_perms)!=req_perms)
        return 0;
    ROSE_ASSERT(va >= m->get_va());
    size_t m_offset = va - m->get_va();
    ROSE_ASSERT(m_offset < m->get_size());
    size_t n = std::min(nbytes, m->get_size()-m_offset);
    memcpy((uint8_t*)m->get_base()+m->get_offset()+m_offset, src_buf, n);
    return n;
}

size_t
MemoryMap::write(const void *src_buf, rose_addr_t start_va, size_t nbytes, unsigned req_perms/*=MM_PROT_WRITE*/) const
{
    size_t total_copied = 0;
    while (total_copied < nbytes) {
        size_t n = write1((uint8_t*)src_buf+total_copied, start_va+total_copied, nbytes-total_copied, req_perms, NULL);
        if (!n) break;
        total_copied += n;
    }
    return total_copied;
}

void
MemoryMap::mprotect(const MapElement &region, bool relax/*=false*/)
{
    /* Check whether the region refers to addresses not in the memory map. */
    if (!relax) {
        ExtentMap e;
        e.insert(Extent(region.get_va(), region.get_size()));
        e.erase_ranges(va_extents());
        if (!e.empty())
            throw NotMapped(this, e.begin()->first.first());
    }

    std::vector<MapElement> created;
    std::vector<MapElement>::iterator i=elements.begin();
    while (i!=elements.end()) {
        MapElement &other = *i;
        if (other.get_mapperms()==region.get_mapperms()) {
            /* no change */
            i++;
        } else if (other.get_va() >= region.get_va()) {
            if (other.get_va()+other.get_size() <= region.get_va()+region.get_size()) {
                /* other is fully contained in (or congruent to) region; change other's permissions */
                other.set_mapperms(region.get_mapperms());
                i++;
            } else if (other.get_va() < region.get_va()+region.get_size()) {
                /* left part of other is contained in region; split other into two parts */
                size_t left_sz = region.get_va() + region.get_size() - other.get_va();
                ROSE_ASSERT(left_sz>0);
                MapElement left = other;
                left.set_size(left_sz);
                left.set_mapperms(region.get_mapperms());
                created.push_back(left);

                size_t right_sz = other.get_size() - left_sz;
                MapElement right = other;
                ROSE_ASSERT(right_sz>0);
                right.set_va(other.get_va() + left_sz);
                right.set_offset(right.get_offset() + left_sz);
                right.set_size(right_sz);
                created.push_back(right);

                i = elements.erase(i);
            } else {
                /* other is right of region; skip it */
                i++;
            }
        } else if (other.get_va()+other.get_size() <= region.get_va()) {
            /* other is left of desired region; skip it */
            i++;
        } else if (other.get_va()+other.get_size() <= region.get_va() + region.get_size()) {
            /* right part of other is contained in region; split other into two parts */
            size_t left_sz = region.get_va() - other.get_va();
            ROSE_ASSERT(left_sz>0);
            MapElement left = other;
            left.set_size(left_sz);
            created.push_back(left);

            size_t right_sz = other.get_size() - left_sz;
            MapElement right = other;
            right.set_va(other.get_va() + left_sz);
            right.set_offset(right.get_offset() + left_sz);
            right.set_size(right_sz);
            right.set_mapperms(region.get_mapperms());
            created.push_back(right);

            i = elements.erase(i);
        } else {
            /* other contains entire region and extends left and right; split into three parts */
            size_t left_sz = region.get_va() - other.get_va();
            ROSE_ASSERT(left_sz>0);
            MapElement left = other;
            left.set_size(left_sz);
            created.push_back(left);
            
            size_t mid_sz = region.get_size();
            ROSE_ASSERT(mid_sz>0);
            MapElement mid = other;
            mid.set_va(region.get_va());
            mid.set_offset(mid.get_offset() + left_sz);
            mid.set_size(region.get_size());
            mid.set_mapperms(region.get_mapperms());
            created.push_back(mid);
            
            size_t right_sz = other.get_size() - (left_sz + mid_sz);
            ROSE_ASSERT(right_sz>0);
            MapElement right = other;
            right.set_va(region.get_va()+region.get_size());
            right.set_offset(mid.get_offset() + mid_sz);
            right.set_size(right_sz);
            created.push_back(right);
            
            i = elements.erase(i);
        }
    }

    elements.insert(elements.end(), created.begin(), created.end());
    if (!created.empty())
        std::sort(elements.begin(), elements.end());
}

ExtentMap
MemoryMap::va_extents() const
{
    ExtentMap retval;
    for (size_t i=0; i<elements.size(); i++) {
        const MapElement& me = elements[i];
        retval.insert(Extent(me.get_va(), me.get_size()));
    }
    return retval;
}

void
MemoryMap::dump(FILE *f, const char *prefix) const
{
    if (0==elements.size())
        fprintf(f, "%sempty\n", prefix);

    std::map<void*,std::string> bases;
    for (size_t i=0; i<elements.size(); i++) {
        const MapElement &me = elements[i];
        std::string basename;

        /* Convert the base address to a unique name like "aaa", "aab", "aac", etc. This makes it easier to compare outputs
         * from different runs since the base addresses are likely to be different between runs but the names aren't. */
        if (me.is_anonymous()) {
            basename = "anon ";
        } else {
            basename = "base ";
        }

        if (NULL==me.get_base(false)) {
            basename += "null";
        } else {
            std::map<void*,std::string>::iterator found = bases.find(me.get_base());
            if (found==bases.end()) {
                size_t j = bases.size();
                ROSE_ASSERT(j<26*26*26);
                basename += 'a'+(j/(26*26))%26;
                basename += 'a'+(j/26)%26;
                basename += 'a'+(j%26);
                bases.insert(std::make_pair(me.get_base(), basename));
            } else {
                basename = found->second;
            }
        }

        fprintf(f, "%sva 0x%08"PRIx64" + 0x%08zx = 0x%08"PRIx64" %c%c%c%c at %-9s + 0x%08"PRIx64,
                prefix, me.get_va(), me.get_size(), me.get_va()+me.get_size(),
                0==(me.get_mapperms()&MM_PROT_READ) ?'-':'r',
                0==(me.get_mapperms()&MM_PROT_WRITE)?'-':'w',
                0==(me.get_mapperms()&MM_PROT_EXEC) ?'-':'x',
                0==(me.get_mapperms()&MM_PROT_PRIVATE)?'-':'p',
                basename.c_str(), elements[i].get_offset());

        if (!me.name.empty()) {
            static const size_t limit = 55;
            std::string name = escapeString(me.get_name());
            if (name.size()>limit)
                name = name.substr(0, limit-3) + "...";
            fprintf(f, " %s", name.c_str());
        }

        fputc('\n', f);
    }
}

void
MemoryMap::dump(const std::string &basename) const
{
    FILE *f = fopen((basename+".index").c_str(), "w");
    ROSE_ASSERT(f!=NULL);
    dump(f);
    fclose(f);

    for (size_t i=0; i<elements.size(); i++) {
        const MapElement &me = elements[i];

        char ext[256];
        sprintf(ext, "-%08"PRIx64".data", me.get_va());
#ifdef _MSC_VER
        int fd = _open((basename+ext).c_str(), O_CREAT|O_TRUNC|O_RDWR, 0666);
#else
        int fd = open((basename+ext).c_str(), O_CREAT|O_TRUNC|O_RDWR, 0666);
#endif
        ROSE_ASSERT(fd>0);
        if (me.is_anonymous() && NULL==me.get_base(false)) {
            /* anonymous and no memory allocated. Zero-fill the file */
            ROSE_ASSERT(me.get_size()>0);
#ifdef _MSC_VER
            off_t offset = _lseek(fd, me.get_size()-1, SEEK_SET);
#else
            off_t offset = lseek(fd, me.get_size()-1, SEEK_SET);
#endif
            ROSE_ASSERT((size_t)offset==me.get_size()-1);
            const int zero = 0;
#ifdef _MSC_VER
            ssize_t n = _write(fd, &zero, 1);
#else
            ssize_t n = ::write(fd, &zero, 1);
#endif
            ROSE_ASSERT(1==n);
        } else {
            const char *ptr = (const char*)me.get_base() + me.get_offset();
            size_t nremain = me.get_size();
            while (nremain>0) {
#ifdef _MSC_VER
                ssize_t n = 0; 
                // todo
                ROSE_ASSERT(false);
#else
                ssize_t n = TEMP_FAILURE_RETRY(::write(fd, ptr, nremain));
#endif
                if (n<0) perror("MemoryMap::dump: write() failed");
                ROSE_ASSERT(n>0);
                ptr += n;
                nremain -= n;
            }
        }
#ifdef _MSC_VER
        close(fd);
#else
        close(fd);
#endif
    }
}

bool
MemoryMap::load(const std::string &basename)
{
    std::string indexname = basename + ".index";
    FILE *f = fopen(indexname.c_str(), "r");
    if (!f) return false;

    char *line = NULL;
    size_t line_nalloc = 0;
    ssize_t nread;
    unsigned nlines=0;
    try {
        while (
#ifndef _MSC_VER
               0<(nread=rose_getline(&line, &line_nalloc, f))
#else
               // error LNK2019: unresolved external symbol "long __cdecl rose_getline(char * *,unsigned int *,struct _iobuf *)"
               ROSE_ASSERT(false), true
#endif
               ) {
            char *rest, *s=line;
            nlines++;

            /* Check for empty lines and comments */
            while (isspace(*s)) s++;
            if (!*s || '#'==*s) continue;

            /* Starting virtual address with optional "va " prefix */
            if (!strncmp(s, "va ", 3)) s += 3;
            errno = 0;
#ifndef _MSC_VER
            rose_addr_t va = strtoull(s, &rest, 0);
#else
            rose_addr_t va = 0;
            ROSE_ASSERT(false);
#endif
            if (rest==s || errno)
                throw Syntax(this, "starting virtual address expected", indexname, nlines, s-line);
            s = rest;

            /* Size, prefixed with optional "+" or "," */
            while (isspace(*s)) s++;
            if ('+'==*s || ','==*s) s++;
            while (isspace(*s)) s++;
            errno = 0;
#ifndef _MSC_VER
            rose_addr_t sz = strtoull(s, &rest, 0);
#else
            rose_addr_t sz = 0;
            ROSE_ASSERT(false);
#endif
            if (rest==s || errno)
                throw Syntax(this, "virtual size expected", indexname, nlines, s-line);
            s = rest;

            /* Optional ending virtual address prefixed with "=" */
            while (isspace(*s)) s++;
            if ('='==*s) {
                s++;
                errno = 0;
#ifndef _MSC_VER
                (void)strtoull(s, &rest, 0);
#else
                ROSE_ASSERT(false);
#endif
                if (rest==s || errno)
                    throw Syntax(this, "ending virtual address expected after '='", indexname, nlines, s-line);
                s = rest;
            }

            /* Permissions with optional ',' prefix. Permissions are the letters 'r', 'w', and/or 'x'. Hyphens can appear in the
             * r/w/x string at any position and are ignored. */
            while (isspace(*s)) s++;
            if (','==*s) s++;
            while (isspace(*s)) s++;
            unsigned perm = 0;
            while (strchr("rwxp-", *s)) {
                switch (*s++) {
                    case 'r': perm |= MM_PROT_READ; break;
                    case 'w': perm |= MM_PROT_WRITE; break;
                    case 'x': perm |= MM_PROT_EXEC; break;
                    case 'p': perm |= MM_PROT_PRIVATE; break;
                    case '-': break;
                    default: break; /*to suppress a compiler warning*/
                }
            }

            /* Base address, "anonymous", or file name.  Base address is introduced with the word "base", file names are
             * anything up the the next space character. An optional "at" prefix is allowed. */
            while (isspace(*s)) s++;
            if (','==*s) s++;
            while (isspace(*s)) s++;
            if (!strncmp(s, "at", 2) && isspace(s[2])) s+= 3;
            while (isspace(*s)) s++;
            bool is_base=false;
            if (!strncmp(s, "base", 4) && isspace(s[4])) {
                s += 5;
                is_base = true;
            }
            while (isspace(*s)) s++;
            char *s2=s;
            while (*s2 && !isspace(*s2)) s2++;
            if (s2==s)
                throw Syntax(this, "data source name expected", indexname, nlines, s-line);
            std::string region_name(s, s2-s);
            s = s2;

            /* Offset from base address; optional prefix of "," or "+". */
            while (isspace(*s)) s++;
            if (','==*s || '+'==*s) s++;
            while (isspace(*s)) s++;
            errno = 0;
#ifndef _MSC_VER
            rose_addr_t offset = strtoull(s, &rest, 0);
#else
            rose_addr_t offset = 0;
            ROSE_ASSERT(false);
#endif
            if (rest==s || errno)
                throw Syntax(this, "file offset expected", indexname, nlines, s-line);
            s = rest;

            /* Comment (optional) */
            while (isspace(*s)) s++;
            if (','==*s) s++;
            while (isspace(*s)) s++;
            char *end = s + strlen(s);
            while (end>s && isspace(end[-1])) --end;
            std::string comment(s, end-s);

            /* Create the map element */
            MapElement me;
            uint8_t *buf = NULL;
            size_t nread=0;
            if (region_name == "anonymous") {
                me = MapElement(va, sz, perm);
                nread = sz;
            } else {
                std::string filename;
                if (is_base) {
                    char ext[256];
                    sprintf(ext, "-%08"PRIx64".data", va);
                    filename = basename+ext;
                } else if ('/'==region_name[0]) {
                    filename = region_name;
                } else {
                    std::string::size_type slash = basename.find_last_of("/");
                    filename = slash==basename.npos ? "." : basename.substr(0, slash);
                    filename += "/" + region_name;
                }
                

                buf = new uint8_t[sz];
                int fd = open(filename.c_str(), O_RDONLY);
                if (fd<0)
                    throw Syntax(this, "cannot open "+filename+": "+strerror(errno), indexname, nlines);
                off_t position = lseek(fd, offset, SEEK_SET);
                ROSE_ASSERT(position!=-1 && position==(off_t)offset);
                while (nread<sz) {
#ifndef _MSC_VER
                    ssize_t n = TEMP_FAILURE_RETRY(::read(fd, buf+nread, sz-nread));
#else
                    ssize_t n= 0;
                    ROSE_ASSERT(false);
#endif

                    if (n<0) {
                        close(fd);
                        throw Syntax(this, "read failed from "+filename+": "+strerror(errno), indexname, nlines);
                    }
                    if (0==n) break;
                    nread += n;
                }
                close(fd);
                me = MapElement(va, nread, buf, 0, perm);
            }
            me.set_name(comment);

            /* Add map element to this memory map. */
            try {
                insert(me);
            } catch (const Exception&) {
                delete[] buf;
                throw;
            }
            if (sz>nread) {
                MapElement me2(va+nread, sz-nread, perm);
                me2.set_name(comment);
                insert(me2);
            }
        }
    } catch (...) {
        fclose(f);
        if (line) free(line);
        throw;
    }

    fclose(f);
    if (line) free(line);
    return nread<=0;
}
