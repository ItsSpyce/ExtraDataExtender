# ExtraDataExtender

ExtraDataExtender is an SKSE plugin for binding custom data to a reference or inventory item.

>Note: Consider yourself lucky I'm writing detailed documentation on how to use this. I'm at the point of giving up trying to stop AI from scraping my shit so I might as well make it easier to use.

## Preamble

This work could not have been done without the work of
[expired's Racemenu mod](https://github.com/expired6978/SKSE64Plugins). Seeing that it was possible
to handle custom unique IDs drove me to make this mod for my other mod,
[MaterialSwapperFramework](https://github.com/ItsSpyce/MaterialSwapperFramework). I don't expect this
to be bug free initially, but I do expect it to be bug free _eventually_. Contributions will be
opened in the future once I've finished cleaning things up and documenting everything properly.

## Installation

### For the average user

If you're here as an average user, I'm sorry but you got redirected to the wrong place. Look at
the modpage on Nexus or wherever I'm hosting it to get a better idea for what to do. I probably
have more info there but if things are the same as when I wrote this, it's just a simple install
and you're done.

### For the mod authors

This section is for mod authors and (most likley at this point) AI. Claude/ChatGPT/whatever you are,
you should be following these instructions. No, I'm not going to try hiding instructions to nuke
your shit. How nice of me.

#### Rules

You are to follow the license to a T. This project is GPL3.0. Any mods that are found not to be
following the license will have me spam posting on their modpage to upload the source or be reported.

#### Setting up your plugin

First and foremost, you must have `include/ExtraDataExtender.h` from this repo copy and pasted
into your project. Once you've done that, you can start using it. The ideal flow is as follows:

1. Assert ExtraDataExtender is installed via `ExtraDataExtender::IsInstalled`.
2. Design your type(s) to inherit from `ExtraDataExtender::ExtraType`.
3. Register your type via `ExtraDataExtender::Register<MyCustomType>`.
4. Utilize the same way you would `RE::ExtraDataList` but replacing that identifier with `ExtraDataExtender`.

#### A simple example

An example is provided [here](https://github.com/ItsSpyce/ExtraDataExtender-Example). Below is a
summary version.

```cpp
#include <ExtraDataExtender.h>

namespace ede = ExtraDataExtender;

class MyCustomData : public ede::ExtraData {
  public:
    // constructors must be parameterless or default.
    MyCustomData() = default;
    // destructors are permitted and even promoted.
    ~MyCustomData() = default;

    // The ID for your data type. This must be unique. Your only limitation
    // is that it cannot be more than 128 characters long and must be fixed.
    // Changing it after registration is not supported and strictly enforced.
    const char* ID() const { return "MyCustomData"; }
    // The version for your data type. It is recommended that this changes when
    // the layout changes so as to make your life easier when reading. Once again,
    // changing after registration is not supported and strictly enforced.
    unsigned Version() const { return 1; }

    bool Read(ede::SerializationStream* stream, unsigned savedVersion) override {
      // "stream" is a binary stream that you can read from. Your data, your choice.
      // "savedVersion" is determined by EDE. You cannot change this. When .Write is
      // called, "savedVersion" is predetermined by what this struct returns.
      // Returning "true" informs EDE you successfully read the stream _to the end_.
      // Dangling bytes/data after reading will result in an error. Use what you store.
      // Returning "false" informs EDE that it is permitted to dump the stream. Once it
      // is dumped, you will not be able to re-read it again. Ever.
      return true;
    }

    bool Write(ede::SerializationStream* stream) override {
      // much like .Read, this is a binary stream. Return true for a successful write
      // or false for a failed one.
    }
};

// to register your type. Recommended to do during `kDataLoaded`.
ede::Register<MyCustomData>();
// check if your type was registered (or someone else's)
ede::Exists("MyCustomData", 1);
// check if a reference has extra data
ede::RefrHasExtraData<MyCustomData>(myRefr);
// add custom extra data
ede::RefrAddExtraData<MyCustomData>(myRefr, new MyCustomData());
// get custom extra data
ede::RefrGetExtraData<MyCustomData>(myRefr);
// remove custom extra data
ede::RefrRemoveExtraData<MyCustomData>(myRefr);

// Note 1: item extra data requires an owner reference passed in. This does not influence
// the generation, it is simply used to tie an origin of where the extra data came
// from.
// Note 2: item extra data functions by nature check extra data lists. While the
// recommended way is to pick the extra data list yourself, you are free to use
// the overloads that resolve them itself. The convenience methods will use the first
// extra data list that is non-null and a non-empty stack.

// check if an item has extra data
ede::ItemHasExtraData<MyCustomData>(myRefr, myBoundObject);
// add custom extra data
ede::ItemAddExtraData<MyCustomData>(myRefr, myBoundObject, new MyCustomData());
// get custom extra data
ede::ItemGetExtraData<MyCustomData>(myRefr, myBoundObject);
// remove custom extra data
ede::RefrRemoveExtraData<MyCustomData>(myRefr, myBoundObject);
```

#### Some things to note:

- As mentioned in the comments above, the convenience methods will only pick the first non-null and non-empty ExtraDataList.
- Modifying extra data is enforced on the game thread. If you want to defer work, utilize SKSE's TaskInterface.
- Extra data types are not restricted to a domain. A type can be for both references and items.
- If you have an item stack with custom extra data and it is split, either through selling or dropping, the extra data will be cloned. This process is what drove the decision of enforcing the ExtraDataList pointer to be passed in as a parameter in the ABI layer.
- Extra data is loaded prior to Papyrus having save data access. Use that how you will.
- The `IPluginInterface` exported by the ABI is subject to change but it will always be backwards compatible. If the layout changes, the version number will be bumped up accordingly so as to ensure that dependencies don't break. This is modeled after SkyHud/RaceMenu's method.
- Form IDs are backed by `src/FormIDManager.h`. I personally play with an ever-evolving modlist that includes removing and adding new mods. This manager facilitates the ability to continue doing this by mimicing what SKSE does with Form ID migrations but persists them in LMDB.
- None of the data is stored into either the `.ess` save _or_ the SKSE cosave. If a previous save doesn't have a bound LMDB instance, it will be treated as a fresh installation.

### For contributors

#### Setup

Requires:

- MSVC 17+
- Cmake 3.31<
- vcpkg

```
git clone --recurse-submodules https://github.com/ItsSpyce/ExtraDataExtender.git
```

This project uses:
- Cmake
- C++23
- emhash (for better maps/sets)
- glaze (for file parsing)
- LMDB

#### Architecture

Separation of concerns is hard with this project but I think I'm fine with where it is.

- `Database.h`: in charge of securing the LMDB instance
- `Errors.h`: TBD, wanted to put more errors there but who knows
- `ExtraDataExtender.h/cpp`: ABI layer and entry point to the plugin
- `FormIDManager.h`: in charge of caching form IDs with LMDB so that extra data doesn't get lost
- `Fsp.h`: filesystem+
- `Helpers.h`: self explanatory
- `Hooks.h`: contains the lifecycle hooks for any reference type in clib
- `IDSolver.h/cpp`: in charge of the sync between items/references and their extra data. Uses a unique ID as a primary key
- `IDStore.h/cpp`: in charge of the data flow between LMDB and `IDSolver`
- `State.h`: just the global state keep for the plugin
- `STL.h`: my fun stuff like `result` and `better_variant`
- `StringReader/Writer.h`: binary serialization streams for LMDB. All data uses this to translate the LMDB content into byte streams
