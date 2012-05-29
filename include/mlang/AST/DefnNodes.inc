#ifndef ABSTRACT_DEFN
#  define ABSTRACT_DEFN(Type) Type
#endif

#ifndef DEFN_RANGE
#  define DEFN_RANGE(Base, First, Last)
#endif

#ifndef LAST_DEFN_RANGE
#  define LAST_DEFN_RANGE(Base, First, Last) DEFN_RANGE(Base, First, Last)
#endif

#ifndef NAMED
#  define NAMED(Type, Base) DEFN(Type, Base)
#endif
ABSTRACT_DEFN(NAMED(Named, Defn))

#ifndef TYPE
#  define TYPE(Type, Base) NAMED(Type, Base)
#endif
ABSTRACT_DEFN(TYPE(Type, NamedDefn))

#ifndef STRUCT
#  define STRUCT(Type, Base) TYPE(Type, Base)
#endif
STRUCT(Struct, TypeDefn)
#undef STRUCT

#ifndef CELL
#  define CELL(Type, Base) TYPE(Type, Base)
#endif
CELL(Cell, TypeDefn)
#undef CELL

#ifndef USERCLASS
#  define USERCLASS(Type, Base) TYPE(Type, Base)
#endif
USERCLASS(UserClass, TypeDefn)
#undef USERCLASS

DEFN_RANGE(Type, Struct, UserClass)
#undef TYPE

#ifndef VALUE
#  define VALUE(Type, Base) NAMED(Type, Base)
#endif
ABSTRACT_DEFN(VALUE(Value, NamedDefn))

#ifndef VAR
#  define VAR(Type, Base) VALUE(Type, Base)
#endif
VAR(Var, ValueDefn)

#ifndef USEROBJECT
#  define USEROBJECT(Type, Base) VAR(Type, Base)
#endif
USEROBJECT(UserObject, VarDefn)
#undef USEROBJECT

#ifndef CLASSPROPERTY
#  define CLASSPROPERTY(Type, Base) VAR(Type, Base)
#endif
CLASSPROPERTY(ClassProperty, VarDefn)
#undef CLASSPROPERTY

#ifndef CLASSEVENT
#  define CLASSEVENT(Type, Base) VAR(Type, Base)
#endif
CLASSEVENT(ClassEvent, VarDefn)
#undef CLASSEVENT

#ifndef PARAMVAR
#  define PARAMVAR(Type, Base) VAR(Type, Base)
#endif
PARAMVAR(ParamVar, VarDefn)
#undef PARAMVAR

#ifndef IMPLICITPARAM
#  define IMPLICITPARAM(Type, Base) VAR(Type, Base)
#endif
IMPLICITPARAM(ImplicitParam, VarDefn)
#undef IMPLICITPARAM

DEFN_RANGE(Var, Var, ImplicitParam)
#undef VAR

#ifndef MEMBER
#  define MEMBER(Type, Base) VALUE(Type, Base)
#endif
MEMBER(Member, ValueDefn)
#undef MEMBER

#ifndef FUNCTION
#  define FUNCTION(Type, Base) VALUE(Type, Base)
#endif
FUNCTION(Function, ValueDefn)

#ifndef CLASSMETHOD
#  define CLASSMETHOD(Type, Base) FUNCTION(Type, Base)
#endif
CLASSMETHOD(ClassMethod, FunctionDefn)

#ifndef CLASSCONSTRUCTOR
#  define CLASSCONSTRUCTOR(Type, Base) CLASSMETHOD(Type, Base)
#endif
CLASSCONSTRUCTOR(ClassConstructor, ClassMethodDefn)
#undef CLASSCONSTRUCTOR

#ifndef CLASSDESTRUCTOR
#  define CLASSDESTRUCTOR(Type, Base) CLASSMETHOD(Type, Base)
#endif
CLASSDESTRUCTOR(ClassDestructor, ClassMethodDefn)
#undef CLASSDESTRUCTOR

DEFN_RANGE(ClassMethod, ClassMethod, ClassDestructor)
#undef CLASSMETHOD

DEFN_RANGE(Function, Function, ClassDestructor)
#undef FUNCTION

DEFN_RANGE(Value, Var, ClassDestructor)
#undef VALUE

#ifndef SCRIPT
#  define SCRIPT(Type, Base) NAMED(Type, Base)
#endif
SCRIPT(Script, NamedDefn)
#undef SCRIPT

#ifndef NAMESPACE
#  define NAMESPACE(Type, Base) NAMED(Type, Base)
#endif
NAMESPACE(Namespace, NamedDefn)
#undef NAMESPACE

DEFN_RANGE(Named, UserClass, Namespace)
#undef NAMED

#ifndef ANONFUNCTION
#  define ANONFUNCTION(Type, Base) DEFN(Type, Base)
#endif
ANONFUNCTION(AnonFunction, Defn)
#undef ANONFUNCTION

#ifndef CLASSATTRIBUTES
#  define CLASSATTRIBUTES(Type, Base) DEFN(Type, Base)
#endif
CLASSATTRIBUTES(ClassAttributes, Defn)

#ifndef CLASSPROPERTIESATTRS
#  define CLASSPROPERTIESATTRS(Type, Base) CLASSATTRIBUTES(Type, Base)
#endif
CLASSPROPERTIESATTRS(ClassPropertiesAttrs, ClassAttributesDefn)
#undef CLASSPROPERTIESATTRS

#ifndef CLASSMETHODSATTRS
#  define CLASSMETHODSATTRS(Type, Base) CLASSATTRIBUTES(Type, Base)
#endif
CLASSMETHODSATTRS(ClassMethodsAttrs, ClassAttributesDefn)
#undef CLASSMETHODSATTRS

#ifndef CLASSEVENTSATTRS
#  define CLASSEVENTSATTRS(Type, Base) CLASSATTRIBUTES(Type, Base)
#endif
CLASSEVENTSATTRS(ClassEventsAttrs, ClassAttributesDefn)
#undef CLASSEVENTSATTRS

DEFN_RANGE(ClassAttributes, ClassAttributes, ClassEventsAttrs)
#undef CLASSATTRIBUTES

#ifndef TRANSLATIONUNIT
#  define TRANSLATIONUNIT(Type, Base) DEFN(Type, Base)
#endif
TRANSLATIONUNIT(TranslationUnit, Defn)
#undef TRANSLATIONUNIT

LAST_DEFN_RANGE(Defn, UserClass, TranslationUnit)

#undef DEFN
#undef DEFN_RANGE
#undef LAST_DEFN_RANGE
#undef ABSTRACT_DEFN


#ifndef DEFN_CONTEXT
#  define DEFN_CONTEXT(DEFN)
#endif
#ifndef DEFN_CONTEXT_BASE
#  define DEFN_CONTEXT_BASE(DEFN) DEFN_CONTEXT(DEFN)
#endif
DEFN_CONTEXT(Namespace)
DEFN_CONTEXT_BASE(Function)
DEFN_CONTEXT_BASE(Type)
DEFN_CONTEXT(TranslationUnit)
DEFN_CONTEXT(UserClass)
DEFN_CONTEXT(Script)
#undef DEFN_CONTEXT
#undef DEFN_CONTEXT_BASE
