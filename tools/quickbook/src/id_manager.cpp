/*=============================================================================
    Copyright (c) 2011 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "id_manager.hpp"
#include "utils.hpp"
#include "string_ref.hpp"
#include <boost/intrusive_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/foreach.hpp>
#include <deque>
#include <vector>
#include <cctype>

namespace quickbook
{
    //
    // Forward declarations
    //

    struct id_placeholder;
    struct id_data;
    std::string process_ids(id_state&, std::string const&);

    static const std::size_t max_size = 32;

    //
    // instructive_base
    //
    // I should probably make this a recursive template, or use SFINAE to check
    // the release is correct, or maybe use a virtual destructor. But I'm
    // feeling reckless.
    //

    struct intrusive_base
    {
        intrusive_base() : ref_count_(0) {}
        intrusive_base(intrusive_base const&) : ref_count_(0) {}
        intrusive_base& operator=(intrusive_base const&) { return *this; }
        ~intrusive_base() { assert(!ref_count_); }

        unsigned ref_count_;

        friend void intrusive_ptr_add_ref(intrusive_base* ptr)
            { ++ptr->ref_count_; }

        template <typename T>
        friend void intrusive_ptr_release(T* ptr)
            { if(--ptr->ref_count_ == 0) delete ptr; }
    private:
    };

    //
    // id_placeholder
    //

    struct id_placeholder
    {
        enum state_enum { child, unresolved, resolved, generated };

        unsigned index;         // The poisition in the placeholder deque.
        state_enum generation_state;
                                // Placeholder's position in generation
                                // process.
        std::string id;         // The id so far.
        id_placeholder* parent; // Placeholder of the parent id.
                                // Only when generation_state == child
        id_category category;
        unsigned num_dots;      // Number of dots in the id.
                                // Normally equal to the section level.
        unsigned order;         // Order of the placeholders in the generated
                                // xml. Stored because it can be slightly
                                // different to the order they're generated
                                // in. e.g. for nested tables the cells
                                // are processed before the title id.
                                // Only set when processing ids.
        id_data* data;          // Assigned data shared by duplicate ids
                                // used to detect them. Only when
                                // generation_state == resolved

        id_placeholder(
                unsigned index,
                std::string const& id,
                id_category category,
                id_placeholder* parent_ = 0)
          : index(index),
            generation_state(parent_ ? child : unresolved),
            id(id),
            parent(parent_),
            category(category),
            num_dots(boost::range::count(id, '.') +
                (parent_ ? parent_->num_dots + 1 : 0)),
            order(0),
            data(0)
        {
        }

        std::string to_string()
        {
            return '$' + boost::lexical_cast<std::string>(index);
        }

        bool check_state() const
        {
            return (
                (generation_state == child) == (bool) parent &&
                (generation_state == resolved) == (bool) data);
        }

        bool check_state(state_enum s) const
        {
            return s == generation_state && check_state();
        }
    };

    //
    // id_state
    //

    struct file_info;
    struct doc_info;
    struct section_info;

    struct id_state
    {
        boost::intrusive_ptr<file_info> current_file;

        std::deque<id_placeholder> placeholders;

        // Placeholder methods

        id_placeholder* add_placeholder(
                std::string const&, id_category, id_placeholder* parent = 0);

        id_placeholder* get_placeholder(string_ref);

        // Events

        std::string start_file(
                std::string const& include_doc_id,
                std::string const& id,
                value const& title);

        void end_file();

        std::string add_id(
                std::string const& id,
                id_category category);
        id_placeholder* add_id_impl(
                std::string const& id,
                id_category category,
                boost::intrusive_ptr<section_info> const& section);
        std::string old_style_id(
            std::string const& id,
            id_category category);
        std::string begin_section(
                std::string const& id,
                id_category category);
        void end_section();

        // Used to change section for the duration of a file.
        // (or until 'endsect').

        void switch_section(boost::intrusive_ptr<section_info> const&);
        void restore_section();
    };

    struct file_info : intrusive_base
    {
        boost::intrusive_ptr<file_info> parent;
        boost::intrusive_ptr<doc_info> document;

        bool document_root; // !parent || document != parent->document
        unsigned compatibility_version;
        boost::intrusive_ptr<section_info> switched_section;
        boost::intrusive_ptr<section_info> original_section;

        // The 1.1-1.5 document id would actually change per file due to
        // explicit ids in includes and a bug which would sometimes use the
        // document title instead of the id.
        std::string doc_id_1_1;

        file_info(boost::intrusive_ptr<file_info> const& parent,
                unsigned compatibility_version) :
            parent(parent), document(parent->document), document_root(false),
            compatibility_version(compatibility_version),
            switched_section(), original_section()
        {}

        file_info(boost::intrusive_ptr<file_info> const& parent,
                boost::intrusive_ptr<doc_info> const& document,
                unsigned compatibility_version) :
            parent(parent), document(document), document_root(true),
            compatibility_version(compatibility_version),
            switched_section(), original_section()
        {}
    };

    struct doc_info : intrusive_base
    {
        boost::intrusive_ptr<section_info> current_section;
        std::string last_title_1_1;
        std::string section_id_1_1;

        doc_info() :
            current_section(), last_title_1_1(), section_id_1_1()
        {}
    };

    struct section_info : intrusive_base
    {
        boost::intrusive_ptr<section_info> parent;
        unsigned compatibility_version;
        unsigned level;
        std::string id_1_1;
        id_placeholder* placeholder_1_6;

        section_info(boost::intrusive_ptr<section_info> const& parent,
                unsigned compatibility_version, std::string const& id) :
            parent(parent), compatibility_version(compatibility_version),
            level(parent ? parent->level + 1 : 1),
            id_1_1(), placeholder_1_6(0)
        {
            if (parent && compatibility_version < 106u) {
                id_1_1 = parent->id_1_1;
                if (!id_1_1.empty() && !id.empty())
                    id_1_1 += ".";
                id_1_1 += id;
            }
        }
    };

    //
    // id_state
    //

    id_placeholder* id_state::add_placeholder(
            std::string const& id, id_category category,
            id_placeholder* parent)
    {
        placeholders.push_back(id_placeholder(
            placeholders.size(), id, category, parent));
        return &placeholders.back();
    }

    id_placeholder* id_state::get_placeholder(string_ref value)
    {
        // If this isn't a placeholder id.
        if (value.size() <= 1 || *value.begin() != '$')
            return 0;

        unsigned index = boost::lexical_cast<int>(std::string(
                value.begin() + 1, value.end()));

        return &placeholders.at(index);
    }

    void id_state::switch_section(boost::intrusive_ptr<section_info> const& p)
    {
        assert(!current_file->switched_section);
        current_file->original_section = current_file->document->current_section;
        current_file->document->current_section = current_file->switched_section = p;
    }

    void id_state::restore_section()
    {
        // TODO: Restore when it's a parent
        if (current_file->switched_section &&
                current_file->switched_section == current_file->document->current_section)
            current_file->document->current_section = current_file->original_section;
    }

    // (continued later)

    //
    // id_manager
    //

    id_manager::id_manager()
      : state(new id_state)
    {
    }

    id_manager::~id_manager() {}

    void id_manager::start_file(
            unsigned compatibility_version,
            std::string const& include_doc_id,
            std::string const& id,
            value const& title)
    {
        state->current_file =
            new file_info(state->current_file, compatibility_version);

        state->start_file(include_doc_id, id, title);
    }

    std::string id_manager::start_file_with_docinfo(
            unsigned compatibility_version,
            std::string const& include_doc_id,
            std::string const& id,
            value const& title)
    {
        state->current_file =
            new file_info(state->current_file, new doc_info(),
                compatibility_version);

        std::string x = state->start_file(include_doc_id, id, title);
        return x;
    }

    void id_manager::end_file()
    {
        state->end_file();
        state->current_file = state->current_file->parent;
    }

    std::string id_manager::begin_section(std::string const& id,
            id_category category)
    {
        state->current_file->document->section_id_1_1 = id;
        return state->begin_section(id, category);
    }

    void id_manager::end_section()
    {
        return state->end_section();
    }

    int id_manager::section_level() const
    {
        return state->current_file->document->current_section->level;
    }

    std::string id_manager::old_style_id(std::string const& id, id_category category)
    {
        return state->old_style_id(id, category);
    }

    std::string id_manager::add_id(std::string const& id, id_category category)
    {
        return state->add_id(id, category);
    }

    std::string id_manager::add_anchor(std::string const& id, id_category category)
    {
        return state->add_placeholder(id, category)->to_string();
    }

    std::string id_manager::replace_placeholders(std::string const& xml) const
    {
        assert(!state->current_file);
        return process_ids(*state, xml);
    }

    unsigned id_manager::compatibility_version() const
    {
        return state->current_file->compatibility_version;
    }

    //
    // normalize_id
    //
    // Normalizes generated ids.
    //

    namespace
    {
        std::string normalize_id(
                std::string src_id,
                std::size_t prefix = 0,
                std::size_t size = max_size)
        {
            std::string id;
            id.swap(src_id);

            std::size_t src = prefix;
            std::size_t dst = prefix;
            size += prefix;

            if (src >= id.length()) {
                return id;
            }

            while (src < id.length() && id[src] == '_') {
                ++src;
            }

            if (src >= id.length()) {
                id += '_';
                return id;
            }

            while (src < id.length() && dst < size) {
                if (id[src] == '_') {
                    do {
                        ++src;
                    } while(src < id.length() && id[src] == '_');

                    if (src < id.length()) id[dst++] = '_';
                }
                else {
                    id[dst++] = id[src++];
                }
            }

            id.erase(dst);

            return id;
        }
    }

    std::string id_state::start_file(
            std::string const& include_doc_id,
            std::string const& id,
            value const& title)
    {
        boost::intrusive_ptr<file_info> parent = current_file->parent;

        std::string initial_doc_id;

        if (current_file->document_root ||
            current_file->compatibility_version >= 106u ||
            (parent && parent->compatibility_version >= 106u)) {
            initial_doc_id = !include_doc_id.empty() ? include_doc_id : id;
        }
        else {
            initial_doc_id = !id.empty() ? id : include_doc_id;
        }

        if (current_file->compatibility_version < 106u) {
            // To be backwards compatible this is set even when docinfo is
            // otherwise ignored.
            if (title.check())
                current_file->document->last_title_1_1 =
                    title.get_quickbook();

            current_file->doc_id_1_1 = !initial_doc_id.empty() ? initial_doc_id :
                detail::make_identifier(current_file->document->last_title_1_1);
        }
        else if (parent) {
            current_file->doc_id_1_1 = parent->doc_id_1_1;
        }

        if (current_file->document_root) {
            // TODO: Revise fallback for old versions
            if (!initial_doc_id.empty()) {
                return begin_section(id, id_category::explicit_section_id);
            }
            else if (!title.empty()) {
                return begin_section(
                    detail::make_identifier(title.get_quickbook()),
                    id_category::generated_doc);
            }
            else if (current_file->compatibility_version >= 106u) {
                return begin_section("doc", id_category::numbered);
            }
            else {
                return begin_section("", id_category::generated_doc);
            }
        }
        else if (current_file->compatibility_version >= 106u &&
                !initial_doc_id.empty()) {

            boost::intrusive_ptr<section_info> alt_section =
                new section_info(
                    this->current_file->document->current_section->parent,
                    current_file->compatibility_version, initial_doc_id);

            alt_section->placeholder_1_6 = add_id_impl(initial_doc_id,
                id_category::explicit_section_id,
                boost::intrusive_ptr<section_info>());
            switch_section(alt_section);

            return "";
        }
        else {
            return "";
        }
    }

    void id_state::end_file()
    {
        restore_section();
    }

    std::string id_state::add_id(
            std::string const& id,
            id_category category)
    {
        return add_id_impl(id, category,
            current_file->document->current_section)->to_string();
    }

    id_placeholder* id_state::add_id_impl(
            std::string const& id,
            id_category category,
            boost::intrusive_ptr<section_info> const& section)
    {
        std::string id_part = id;

        // Note: Normalizing id according to file compatibility version, but
        // adding to section according to section compatibility version.

        if (current_file->compatibility_version >= 106u &&
                category.c < id_category::explicit_id) {
            id_part = normalize_id(id);
        }

        if(!section || section->compatibility_version >= 106u) {
            return add_placeholder(id_part, category,
                section ? section->placeholder_1_6 : 0);
        }
        else {
            std::string const& qualified_id = section->id_1_1;

            std::string new_id;
            if (!section->placeholder_1_6)
                new_id = current_file->doc_id_1_1;
            if (!new_id.empty() && !qualified_id.empty()) new_id += '.';
            new_id += qualified_id;
            if (!new_id.empty() && !id_part.empty()) new_id += '.';
            new_id += id_part;

            return add_placeholder(new_id, category,
                section->placeholder_1_6);
        }
    }

    std::string id_state::old_style_id(
        std::string const& id,
        id_category category)
    {
        return current_file->compatibility_version < 103u ?
            add_placeholder(
                current_file->document->section_id_1_1 + "." + id, category)
                    ->to_string() : add_id(id, category);
    }

    std::string id_state::begin_section(
            std::string const& id,
            id_category category)
    {
        // TODO: Begin 1.6 section inside 1.1 section

        boost::intrusive_ptr<section_info> parent =
            current_file->document->current_section;

        boost::intrusive_ptr<section_info> new_section =
            new section_info(parent, current_file->compatibility_version, id);

        id_placeholder* p;

        if (new_section->compatibility_version >= 106u) {
            p = add_id_impl(id, category, parent);
            new_section->placeholder_1_6 = p;
        }
        else if (new_section->compatibility_version >= 103u) {
            if (parent)
                new_section->placeholder_1_6 = parent->placeholder_1_6;

            std::string new_id;
            if (!new_section->placeholder_1_6) {
                new_id = current_file->doc_id_1_1;
                if (!new_section->id_1_1.empty()) new_id += '.';
            }
            new_id += new_section->id_1_1;

            p = add_placeholder(new_id, category,
                new_section->placeholder_1_6);
        }
        else {
            if (parent)
                new_section->placeholder_1_6 = parent->placeholder_1_6;

            std::string new_id;
            if (parent && !new_section->placeholder_1_6)
                new_id = current_file->doc_id_1_1 + '.';

            new_id += id;

            p = add_placeholder(new_id, category,
                new_section->placeholder_1_6);
        }

        current_file->document->current_section = new_section;

        return p->to_string();
    }

    void id_state::end_section()
    {
        current_file->document->current_section =
            current_file->document->current_section->parent;
    }

    //
    // Xml subset parser used for finding id values.
    //
    // I originally tried to integrate this into the post processor
    // but that proved tricky. Alternatively it could use a proper
    // xml parser, but I want this to be able to survive badly
    // marked up escapes.
    //

    struct xml_processor
    {
        xml_processor();

        std::vector<std::string> id_attributes;

        struct callback {
            virtual void start(string_ref) {}
            virtual void id_value(string_ref) {}
            virtual void finish(string_ref) {}
            virtual ~callback() {}
        };

        void parse(std::string const&, callback&);
    };

    namespace
    {
        char const* id_attributes_[] =
        {
            "id",
            "linkend",
            "linkends",
            "arearefs"
        };
    }

    xml_processor::xml_processor()
    {
        static int const n_id_attributes = sizeof(id_attributes_)/sizeof(char const*);
        for (int i = 0; i != n_id_attributes; ++i)
        {
            id_attributes.push_back(id_attributes_[i]);
        }

        boost::sort(id_attributes);
    }

    template <typename Iterator>
    bool read(Iterator& it, Iterator end, char const* text)
    {
        for(Iterator it2 = it;; ++it2, ++text) {
            if (!*text) {
                it = it2;
                return true;
            }

            if (it2 == end || *it2 != *text)
                return false;
        }
    }

    template <typename Iterator>
    void read_past(Iterator& it, Iterator end, char const* text)
    {
        while (it != end && !read(it, end, text)) ++it;
    }

    bool find_char(char const* text, char c)
    {
        for(;*text; ++text)
            if (c == *text) return true;
        return false;
    }

    template <typename Iterator>
    void read_some_of(Iterator& it, Iterator end, char const* text)
    {
        while(it != end && find_char(text, *it)) ++it;
    }

    template <typename Iterator>
    void read_to_one_of(Iterator& it, Iterator end, char const* text)
    {
        while(it != end && !find_char(text, *it)) ++it;
    }

    void xml_processor::parse(std::string const& source, callback& c)
    {
        typedef std::string::const_iterator iterator;

        string_ref source_ref(source.begin(), source.end());
        c.start(source_ref);

        iterator it = source_ref.begin(), end = source_ref.end();

        for(;;)
        {
            read_past(it, end, "<");
            if (it == end) break;

            if (read(it, end, "!--quickbook-escape-prefix-->"))
            {
                read_past(it, end, "<!--quickbook-escape-postfix-->");
                continue;
            }

            switch(*it)
            {
            case '?':
                ++it;
                read_past(it, end, "?>");
                break;

            case '!':
                if (read(it, end, "!--"))
                    read_past(it, end, "-->");
                else
                    read_past(it, end, ">");
                break;

            default:
                if ((*it >= 'a' && *it <= 'z') ||
                        (*it >= 'A' && *it <= 'Z') ||
                        *it == '_' || *it == ':')
                {
                    read_to_one_of(it, end, " \t\n\r>");

                    for (;;) {
                        read_some_of(it, end, " \t\n\r");
                        iterator name_start = it;
                        read_to_one_of(it, end, "= \t\n\r>");
                        if (it == end || *it == '>') break;
                        string_ref name(name_start, it);
                        ++it;

                        read_some_of(it, end, "= \t\n\r");
                        if (it == end || (*it != '"' && *it != '\'')) break;

                        char delim = *it;
                        ++it;

                        iterator value_start = it;

                        it = std::find(it, end, delim);
                        if (it == end) break;
                        string_ref value(value_start, it);
                        ++it;

                        if (boost::find(id_attributes, name)
                                != id_attributes.end())
                        {
                            c.id_value(value);
                        }
                    }
                }
                else
                {
                    read_past(it, end, ">");
                }
            }
        }

        c.finish(source_ref);
    }

    //
    // process_ids
    //

    //
    // Data used for generating placeholders that have duplicates.
    //

    struct id_generation_data
    {
        id_generation_data(std::string const& src_id)
          : child_start(src_id.rfind('.') + 1),
            id(normalize_id(src_id, child_start, max_size - 1)),
                // 'max_size - 1' leaves a character to append
                // a number.
            count(0)
        {
            if (std::isdigit(id[id.length() - 1]))
            {
                if (child_length() < max_size - 1)
                    id += '_';
                else
                    reduce_id();
            }
        }

        void reduce_id()
        {
            assert(id.length() > child_start);
            std::size_t length = id.length() - 1;
            while(length > child_start && std::isdigit(id[length - 1])) --length;
            id.erase(length);
            count = 0;
        }

        std::size_t child_length() const
        {
            return id.length() - child_start;
        }

        std::size_t child_start;
        std::string id;
        int count;
    };

    // Created for all desired ids, either when resolving an id or due to
    // generating a new id to avoid duplicates.
    struct id_data
    {
        id_data()
          : category(id_category::numbered),
            used(false),
            generation_data()
        {}

        void update_category(id_category c)
        {
            if (c.c > category.c) category = c;
        }

        id_category category;   // The highest priority category of the
                                // placeholders that want to use this id.
        bool used;              // Whether this id has been used.
        boost::shared_ptr<id_generation_data> generation_data;
                                // If a duplicates are found, this is
                                // created to generate new ids.
                                //
                                // Many to one relationship, because truncation
                                // can lead to different ids contending for the
                                // same id prefix.
    };

    typedef boost::unordered_map<std::string, id_data> allocated_ids;
    typedef std::vector<id_placeholder*> placeholder_index;

    placeholder_index index_placeholders(id_state&, std::string const& xml);
    void resolve_id(id_placeholder&, allocated_ids&);
    void generate_id(id_placeholder&, allocated_ids&);
    std::string replace_ids(id_state& state, std::string const& xml);

    std::string process_ids(id_state& state, std::string const& xml)
    {
        placeholder_index placeholders = index_placeholders(state, xml);

        typedef std::vector<id_placeholder*>::iterator iterator;

        iterator it = placeholders.begin(),
            end = placeholders.end();

        // Placeholder ids are processed in blocks of ids with
        // an equal number of dots.
        while (it != end) {
            unsigned num_dots = (*it)->num_dots;

            // ids can't clash with ids at a different num_dots, so
            // this only needs to track the id generation data
            // for a single num_dots at a time.
            allocated_ids ids;

            iterator it2 = it;
            do {
                resolve_id(**it2++, ids);
            } while(it2 != end && (*it2)->num_dots == num_dots);

            do {
                generate_id(**it++, ids);
            } while(it != it2);
        }

        return replace_ids(state, xml);
    }

    //
    // index_placeholders
    //
    // Create a sorted index of the placeholders, in order
    // to make numbering duplicates easy. A total order.
    //

    struct placeholder_compare
    {
        bool operator()(id_placeholder* x, id_placeholder* y) const
        {
            bool x_explicit = x->category.c >= id_category::explicit_id;
            bool y_explicit = y->category.c >= id_category::explicit_id;

            return
                x->num_dots < y->num_dots ? true :
                x->num_dots > y->num_dots ? false :
                x_explicit > y_explicit ? true :
                x_explicit < y_explicit ? false :
                x->order < y->order;
        }
    };

    struct number_placeholders_callback : xml_processor::callback
    {
        id_state& state;
        unsigned count;

        number_placeholders_callback(id_state& state)
          : state(state),
            count(0)
        {}

        void id_value(string_ref value)
        {
            id_placeholder* p = state.get_placeholder(value);
            number(p);
        }

        void number(id_placeholder* p)
        {
            if (p && !p->order) {
                number(p->parent);
                p->order = ++count;
            }
        }
    };

    placeholder_index index_placeholders(
            id_state& state,
            std::string const& xml)
    {
        xml_processor processor;
        number_placeholders_callback callback(state);
        processor.parse(xml, callback);

        placeholder_index sorted_placeholders;
        sorted_placeholders.reserve(state.placeholders.size());
        BOOST_FOREACH(id_placeholder& p, state.placeholders)
            if (p.order) sorted_placeholders.push_back(&p);
        boost::sort(sorted_placeholders, placeholder_compare());

        return sorted_placeholders;
    }

    //
    // resolve_id
    //
    // Convert child ids to full ids, and add to the
    // allocated ids (although not yet set in stone because
    // there might be duplicates).
    //
    // Note that the parent ids has to be generated before resolving
    // the child id.
    //

    void resolve_id(id_placeholder& p, allocated_ids& ids)
    {
        if (p.generation_state == id_placeholder::child)
        {
            assert(p.check_state());

            assert(p.parent->check_state(id_placeholder::generated));

            p.id = p.parent->id + "." + p.id;
            p.generation_state = id_placeholder::unresolved;
            p.parent = 0;
        }

        assert(p.check_state(id_placeholder::unresolved));

        id_data& data = ids.emplace(p.id, id_data()).first->second;
        data.update_category(p.category);

        p.data = &data;
        p.generation_state = id_placeholder::resolved;
    }

    //
    // generate_id
    //
    // Finally generate the final id.
    //

    void register_generation_data(id_placeholder&, allocated_ids&);

    void generate_id(id_placeholder& p, allocated_ids& ids)
    {
        assert(p.check_state(id_placeholder::resolved));

        // If the placeholder id is available, then update data
        // and return.
        if (p.category == p.data->category && !p.data->used &&
            p.category.c != id_category::numbered)
        {
            p.data->used = true;
            p.generation_state = id_placeholder::generated;
            p.data = 0;
            return;
        }

        if (!p.data->generation_data)
        {
            p.data->generation_data.reset(new id_generation_data(p.id));
            register_generation_data(p, ids);
        }

        // Loop until an available id is found.
        for(;;)
        {
            id_generation_data& generation_data = *p.data->generation_data;

            std::string postfix =
                boost::lexical_cast<std::string>(generation_data.count++);

            if (generation_data.child_length() + postfix.length() > max_size) {
                // The resulting id is too long, so move to a shorter id.
                generation_data.reduce_id();
                register_generation_data(p, ids);
            }
            else {
                std::string id = generation_data.id + postfix;

                if (ids.find(id) == ids.end()) {
                    p.id.swap(id);
                    p.generation_state = id_placeholder::generated;
                    p.data = 0;
                    return;
                }
            }
        }
    }

    // Every time the generation id is changed, this is called to
    // check if that id is already in use.
    void register_generation_data(id_placeholder& p, allocated_ids& ids)
    {
        std::string const& id = p.data->generation_data->id;

        id_data& new_data = ids.emplace(id, id_data()).first->second;

        // If there is already generation_data for the new id then use that.
        // Otherwise use the placeholder's existing generation_data.
        if (new_data.generation_data)
            p.data->generation_data = new_data.generation_data;
        else
            new_data.generation_data = p.data->generation_data;
    }

    //
    // replace_ids
    //
    // Return a copy of the xml with all the placeholders replaced by
    // generated_ids.
    //

    struct replace_ids_callback : xml_processor::callback
    {
        id_state& state;
        std::string::const_iterator source_pos;
        std::string result;

        replace_ids_callback(id_state& state)
          : state(state),
            source_pos(),
            result()
        {}

        void start(string_ref xml)
        {
            source_pos = xml.begin();
        }

        void id_value(string_ref value)
        {
            if (id_placeholder* p = state.get_placeholder(value))
            {
                assert(p->check_state(id_placeholder::generated));

                result.append(source_pos, value.begin());
                result.append(p->id.begin(), p->id.end());
                source_pos = value.end();
            }
        }

        void finish(string_ref xml)
        {
            result.append(source_pos, xml.end());
            source_pos = xml.end();
        }
    };

    std::string replace_ids(id_state& state, std::string const& xml)
    {
        xml_processor processor;
        replace_ids_callback callback(state);
        processor.parse(xml, callback);
        return callback.result;
    }
}
