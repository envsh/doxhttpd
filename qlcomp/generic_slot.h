#ifndef GENERIC_SLOT_H
#define GENERIC_SLOT_H

#include "compatcore34.h"
#include "generic_slot_base.h"
#include <functional>
#include <memory>
#include <tuple>

// ====== C++11 index_sequence ======
template <size_t... Is> struct seq {};
template <size_t N, size_t... Is> struct gen_seq : gen_seq<N-1, N-1, Is...> {};
template <size_t... Is> struct gen_seq<0, Is...> : seq<Is...> {};

// ====== function_traits ======
template <typename F>
struct function_traits : function_traits<decltype(&F::operator())> {};

template <typename R, typename... Args>
struct function_traits<R(*)(Args...)> {
    static constexpr size_t arity = sizeof...(Args);
    template <size_t I>
    struct arg { using type = typename std::tuple_element<I, std::tuple<Args...>>::type; };
};

template <typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...) const> {
    static constexpr size_t arity = sizeof...(Args);
    template <size_t I>
    struct arg { using type = typename std::tuple_element<I, std::tuple<Args...>>::type; };
};

template <typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...)> {
    static constexpr size_t arity = sizeof...(Args);
    template <size_t I>
    struct arg { using type = typename std::tuple_element<I, std::tuple<Args...>>::type; };
};

// ====== expand_args (Qt4: void** unpack) ======
template <typename F, size_t... Is>
void expand_args(F& f, void** args, seq<Is...>) {
    using T = function_traits<typename std::decay<F>::type>;
    f(*static_cast<typename T::template arg<Is>::type*>(args[Is+1])...);
}

// ====== Qt3 QUObject 提取 ======
#ifdef QT3_BUILD
#include <private/qucom_p.h>

template <typename T>
struct QUObjectAt {
    static T get(QUObject* o) {
        return static_cast<T>(o->payload.ptr);
    }
};
template <> struct QUObjectAt<int> {
    static int get(QUObject* o) { return o->payload.i; }
};
template <> struct QUObjectAt<bool> {
    static bool get(QUObject* o) { return o->payload.b; }
};
template <> struct QUObjectAt<float> {
    static float get(QUObject* o) { return o->payload.f; }
};
template <> struct QUObjectAt<double> {
    static double get(QUObject* o) { return o->payload.d; }
};
template <> struct QUObjectAt<QString> {
    static QString get(QUObject* o) { return *(QString*)o->payload.ptr; }
};
template <> struct QUObjectAt<const char*> {
    static const char* get(QUObject* o) { return o->payload.charstar.ptr; }
};

template <typename F, size_t... Is>
void expand_quobject(F& f, QUObject* o, seq<Is...>) {
    using T = function_traits<typename std::decay<F>::type>;
    f(QUObjectAt<typename T::template arg<Is>::type>::get(&o[Is])...);
}
#endif

// ====== GenericSlot (无 Q_OBJECT, 不被 moc 处理) ======
#ifdef QT3_BUILD
class GenericSlot : public GenericSlotBase {
#else
class GenericSlot : public QObject {
#endif
public:
#ifdef QT3_BUILD
    GenericSlot(QObject* parent, std::function<void(QUObject*)> fn)
        : GenericSlotBase(parent), m_fn(std::move(fn)) {}
    bool qt_invoke(int id, QUObject* o);
#else
    GenericSlot(QObject* parent, std::function<void(void**)> fn)
        : QObject(parent), m_fn(std::move(fn)) {}
    int qt_metacall(QMetaObject::Call c, int id, void** args);
    static int slotMethodIndex() {
        static int idx = QObject::staticMetaObject.methodCount();
        return idx;
    }
#endif

private:
#ifdef QT3_BUILD
    std::function<void(QUObject*)> m_fn;
#else
    std::function<void(void**)> m_fn;
#endif
};

// ====== connect_lambda — Qt5 风格一行式 API ======
/* 用法：
 *   connect_lambda(btn, SIGNAL(clicked()), []{ qDebug("hi"); });
 *   connect_lambda(slider, SIGNAL(valueChanged(int)), [](int v){ ... });
 *   connect_lambda(dlg, SIGNAL(destroyed(QObject*)), []{ s_ptr = 0; });
 */
template <typename F>
bool connect_lambda(QObject* sender, const char* signal, F&& func) {
    using F2 = typename std::decay<F>::type;
    auto sp = std::shared_ptr<F2>(new F2(std::forward<F>(func)));

#ifdef QT3_BUILD
    auto wrapper = [sp](QUObject* o) {
        expand_quobject(*sp, o, gen_seq<function_traits<F2>::arity>{});
    };
    GenericSlot* gs = new GenericSlot(sender, std::move(wrapper));
    return QObject::connect(sender, signal, gs, SLOT(call()));
#else
    QByteArray sig = QMetaObject::normalizedSignature(signal + 1);
    int sigIdx = sender->metaObject()->indexOfSignal(sig);
    if (sigIdx < 0)
        return false;
    auto wrapper = [sp](void** args) {
        expand_args(*sp, args, gen_seq<function_traits<F2>::arity>{});
    };
    GenericSlot* gs = new GenericSlot(sender, std::move(wrapper));
    return QMetaObject::connect(sender, sigIdx, gs, GenericSlot::slotMethodIndex());
#endif
}

#endif
