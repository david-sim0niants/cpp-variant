#include <type_traits>
#include <stdexcept>


template<typename ...Variant_Ts> struct VariantHelper;


template<typename Curr_T, typename ...Rest_Ts>
struct VariantHelper<Curr_T, Rest_Ts...>
{
    inline static void destroy(size_t id, void *data)
    {
        if (id == typeid(Curr_T).hash_code())
        {
            reinterpret_cast<Curr_T *>(data)->~Curr_T();
        }
        else
        {
            VariantHelper<Rest_Ts...>::destroy(id, data);
        }
    }

    inline static void move(size_t old_id, void *old_data, void *new_data)
    {
        if (old_id == typeid(Curr_T).hash_code())
        {
            new (new_data) Curr_T(std::move(*reinterpret_cast<Curr_T *>(old_data)));
        }
        else
        {
            VariantHelper<Rest_Ts...>::move(old_id, old_data, new_data);
        }
    }

    inline static void copy(size_t old_id, void *old_data, void *new_data)
    {
        if (old_id == typeid(Curr_T).hash_code())
        {
            new (new_data) Curr_T(*reinterpret_cast<Curr_T *>(old_data));
        }
        else
        {
            VariantHelper<Rest_Ts...>::copy(old_id, old_data, new_data);
        }
    }
};

template<> struct VariantHelper<>
{
    inline static void destroy(size_t id, void *data) {}
    inline static void move(size_t old_id, void *old_data, void *new_data) {}
    inline static void copy(size_t old_id, void *old_data, void *new_data) {}
};


class variant_error : public std::runtime_error
{
public: explicit variant_error(const char *msg) : std::runtime_error(msg) {}
};

template<typename ...Ts>
struct Variant
{
private:
    template<size_t ...args> struct StaticMax;

    template<size_t arg, size_t ...args> struct StaticMax<arg, args...>
    {
        static constexpr size_t value = StaticMax<args...>::value;
    };

    template<size_t arg> struct StaticMax<arg>
    {
        static constexpr size_t value = arg;
    };


    static constexpr size_t SIZE = StaticMax<sizeof(Ts)...>::value;
    static constexpr size_t ALIGN = StaticMax<alignof(Ts)...>::value;
    using Data = typename std::aligned_storage<SIZE, ALIGN>::type;

    size_t type_id;
    Data data;


    template<typename Type, typename ...Pack> struct FindType;

    template<typename Type, typename Curr_T, typename ...Rest_Ts>
    struct FindType<Type, Curr_T, Rest_Ts...>
    {
        static constexpr bool value = std::is_same<Type, Curr_T>::value || FindType<Type, Rest_Ts...>::value;
    };

    template<typename Type> struct FindType<Type>
    {
        static constexpr bool value = false;
    };

public:
    Variant() : type_id(typeid(void).hash_code()) {}

    Variant(const Variant &other) : type_id(other.type_id)
    {
        VariantHelper<Ts...>::copy(other.type_id, &other.data, &data);
    }

    Variant(Variant &&other) : type_id(other.type_id)
    {
        VariantHelper<Ts...>::move(other.type_id, &other.data, &data);
    }

    Variant& operator=(Variant rhs)
    {
        std::swap(type_id, rhs.type_id);
        std::swap(data, rhs.data);
        return *this;
    }

    ~Variant()
    {
        VariantHelper<Ts...>::destroy(type_id, &data);
    }

    template<typename T>
    inline bool is() { return type_id == typeid(T).hash_code(); }

    template<typename T>
    inline bool valid() { return (type_id != typeid(void).hash_code()); }

    template<typename T>
    T& get()
    {
        if (type_id == typeid(T).hash_code())
        {
            return *reinterpret_cast<T *>(&data);
        }
        else
        {
            throw std::bad_cast();
        }
    }

    template<typename T, typename ...Args>
    void set(Args &&...args)
    {
        if (!FindType<T, Ts...>::value)
            throw variant_error("Couldn't set new value to the variant as it's type wasn't contained by the variant.");

        VariantHelper<Ts...>::destroy(type_id, &data);
        new (&data) T(std::forward(args)...);
        type_id = typeid(T);
    }
};
