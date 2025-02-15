#pragma once

#include "DialogLoadUnload.hpp"
#include "window_dlg_preheat.hpp"
#include "static_alocation_ptr.hpp"
#include <array>

class DialogFactory {
    DialogFactory() = delete;
    DialogFactory(const DialogFactory &) = delete;
    using mem_space = std::aligned_union<0, DialogLoadUnload, DialogMenuPreheat>::type;
    static mem_space all_dialogs;

    //safer than make_static_unique_ptr, checks storage size
    template <class T, class... Args>
    static static_unique_ptr<IDialogMarlin> makePtr(Args &&... args) {
        static_assert(sizeof(T) <= sizeof(all_dialogs), "Error dialog does not fit");
        return make_static_unique_ptr<T>(&all_dialogs, std::forward<Args>(args)...);
    }

public:
    typedef static_unique_ptr<IDialogMarlin> (*fnc)(fsm::BaseData data); //function pointer definition
    using Ctors = std::array<fnc, size_t(ClientFSM::_count)>;
    //define factory methods for all dialogs here
    static static_unique_ptr<IDialogMarlin> load_unload(fsm::BaseData data);
    static static_unique_ptr<IDialogMarlin> Preheat(fsm::BaseData data);
    static static_unique_ptr<IDialogMarlin> screen_not_dialog(fsm::BaseData data);

    static Ctors GetAll(); //returns all factory methods in an array
};
