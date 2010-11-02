################################################################################
#
# utils.py
#
# Various utility functions used by other Python modules.
#
################################################################################

def parse_keywords(kw):
    '''turns a comma-separated list of strings to a list of keywords'''
    list = kw.lower().split(",")
    for i in range(len(list)):
        list[i] = list[i].strip()
    return list

def is_one_keyword(kw, word, abbrev_ok = False):
    '''returns whether or not the single word is a keyword in the list'''
    for one_kw in kw:
        if word == one_kw:
            return True
        elif abbrev_ok and len(one_kw)>len(word) and one_kw[0:len(word)]==word:
            return True
    return False
            
def is_keyword(kw, word, abbrev_ok = False):
    '''returns whether or not the word (or list of words) is a keyword'''
    kw   = parse_keywords(kw)
    word = parse_keywords(word)

    for one_word in word:
        if is_one_keyword(kw, one_word, abbrev_ok):
            return True
    return False

def has_proto(ch, proto):
    '''returns whether or not the character has on his or her person an object
       that inherits from the given prototype'''
    for obj in ch.inv + ch.eq:
        if obj.isinstance(proto):
            return True
    return False

def find_all_objs(looker, list, name, proto = None, must_see = True):
    '''returns a list of all the objects that match the supplied constraints'''
    found = []
    for obj in list:
        if must_see and not looker.cansee(obj):
            continue
        elif name != None and is_keyword(obj.keywords, name, True):
            found.append(obj)
        elif proto != None and obj.isinstance(proto):
            found.append(obj)
    return found

def find_obj(looker, list, num, name, proto = None, must_see = True):
    '''returns the numth object to match the supplied constraints'''
    count = 0
    for obj in list:
        if must_see and not looker.cansee(obj):
            continue
        elif name != None and is_keyword(obj.keywords, name, True):
            count = count + 1
        elif proto != None and obj.isinstance(proto):
            count = count + 1
        if count == num:
            return obj
    return None

def get_count(str):
    '''separates a name and a count, and returns the two'''
    parts = str.lower().split(".", 1)

    # did we get two, or one?
    if len(parts) == 1 and parts[0] == "all":
        return "all", ""
    elif len(parts) == 1:
        return 1, str

    if parts[0] == "all":
        return "all", parts[1]
    try:
        return int(parts[0]), parts[1]
    except:
        return 1, str

def show_list(ch, list, s_func, m_func = None):
    '''shows a list of things to the character. s_func is the description if
       there is only a single item of the type. m_func is the description if
       there are multiple occurences of the thing in the list'''

    # maps descriptions to counts
    counts = { }

    # build up our counts
    for thing in list:
        if counts.has_key(s_func(thing)):
            counts[s_func(thing)] = counts[s_func(thing)] + 1
        else:
            counts[s_func(thing)] = 1

    # print out our counts
    for thing in list:
        # only display > 0 counts. Otherwise it has been displayed
        if counts.has_key(s_func(thing)):
            count = counts.pop(s_func(thing))

            # display our item(s)
            if count == 1:
                ch.send(s_func(thing))
            elif m_func == None:
                ch.send("(" + str(count) + ") " + s_func(thing))
            else:
                ch.send(m_func(thing) % count)
        else: pass