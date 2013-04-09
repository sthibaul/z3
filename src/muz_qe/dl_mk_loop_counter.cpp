/*++
Copyright (c) 2013 Microsoft Corporation

Module Name:

    dl_mk_loop_counter.cpp

Abstract:

    Add loop counter argument to relations.

Author:

    Nikolaj Bjorner (nbjorner) 2013-03-31

Revision History:

--*/

#include"dl_mk_loop_counter.h"
#include"dl_context.h"

namespace datalog {

    mk_loop_counter::mk_loop_counter(context & ctx, unsigned priority):
        plugin(priority),
        m(ctx.get_manager()),
        a(m),
        m_refs(m) {        
    }

    mk_loop_counter::~mk_loop_counter() { }

    app_ref mk_loop_counter::add_arg(app* fn, unsigned idx) {
        expr_ref_vector args(m);
        func_decl* new_fn, *old_fn = fn->get_decl();
        args.append(fn->get_num_args(), fn->get_args());
        args.push_back(m.mk_var(idx, a.mk_int()));
        
        if (!m_old2new.find(old_fn, new_fn)) {
            ptr_vector<sort> domain;
            domain.append(fn->get_num_args(), old_fn->get_domain());
            domain.push_back(a.mk_int());
            new_fn = m.mk_func_decl(old_fn->get_name(), domain.size(), domain.c_ptr(), old_fn->get_range());
            m_old2new.insert(old_fn, new_fn);
            m_new2old.insert(new_fn, old_fn);
            m_refs.push_back(new_fn);
        }
        return app_ref(m.mk_app(new_fn, args.size(), args.c_ptr()), m);
    }
        
    rule_set * mk_loop_counter::operator()(rule_set const & source) {
        m_refs.reset();
        m_old2new.reset();
        m_new2old.reset();
        context& ctx = source.get_context();
        rule_manager& rm = source.get_rule_manager();
        rule_set * result = alloc(rule_set, ctx);
        unsigned sz = source.get_num_rules();
        rule_ref new_rule(rm);
        app_ref_vector tail(m);
        app_ref head(m);
        svector<bool> neg;
        rule_counter& vc = rm.get_counter();
        for (unsigned i = 0; i < sz; ++i) {            
            tail.reset();
            neg.reset();
            rule & r = *source.get_rule(i);
            unsigned cnt = vc.get_max_rule_var(r)+1;
            unsigned utsz = r.get_uninterpreted_tail_size();
            unsigned tsz = r.get_tail_size();
            for (unsigned j = 0; j < utsz; ++j, ++cnt) {
                tail.push_back(add_arg(r.get_tail(j), cnt));                
                neg.push_back(r.is_neg_tail(j));
                ctx.register_predicate(tail.back()->get_decl(), false);
            }
            for (unsigned j = utsz; j < tsz; ++j) {
                tail.push_back(r.get_tail(j));
                neg.push_back(false);
            }
            head = add_arg(r.get_head(), cnt);
            ctx.register_predicate(head->get_decl(), false);
            // set the loop counter to be an increment of the previous 
            bool found = false;
            unsigned last = head->get_num_args()-1;
            for (unsigned j = 0; !found && j < utsz; ++j) {
                if (head->get_decl() == tail[j]->get_decl()) {
                    tail.push_back(m.mk_eq(head->get_arg(last), 
                                           a.mk_add(tail[j]->get_arg(last),
                                                    a.mk_numeral(rational(1), true))));
                    neg.push_back(false);
                    found = true;
                }
            }
            // initialize loop counter to 0 if none was found.
            if (!found) {
                expr_ref_vector args(m);
                args.append(head->get_num_args(), head->get_args());
                args[last] = a.mk_numeral(rational(0), true);
                head = m.mk_app(head->get_decl(), args.size(), args.c_ptr());
            }            

            new_rule = rm.mk(head, tail.size(), tail.c_ptr(), neg.c_ptr(), r.name(), true);
            result->add_rule(new_rule);
        }

        // model converter: remove references to extra argument.
        // proof converter: remove references to extra argument as well.

        return result;
    }

};
