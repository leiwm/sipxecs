/*
 *
 *
 * Copyright (C) 2007 Pingtel Corp., certain elements licensed under a Contributor Agreement.
 * Contributors retain copyright to elements licensed under a Contributor Agreement.
 * Licensed to the User under the LGPL license.
 *
 * $
 */
package org.sipfoundry.sipxconfig.conference;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.apache.commons.lang.RandomStringUtils;
import org.apache.commons.lang.StringUtils;
import org.sipfoundry.sipxconfig.admin.forwarding.AliasMapping;
import org.sipfoundry.sipxconfig.common.NamedObject;
import org.sipfoundry.sipxconfig.common.SipUri;
import org.sipfoundry.sipxconfig.common.User;
import org.sipfoundry.sipxconfig.setting.BeanWithSettings;
import org.sipfoundry.sipxconfig.setting.ProfileNameHandler;
import org.sipfoundry.sipxconfig.setting.Setting;
import org.sipfoundry.sipxconfig.setting.SettingEntry;
import org.sipfoundry.sipxconfig.setting.SettingValue;
import org.sipfoundry.sipxconfig.setting.SettingValueImpl;

public class Conference extends BeanWithSettings implements NamedObject {
    public static final String BEAN_NAME = "conferenceConference";

    /**
     * default lengths of autogenerated access code
     */
    public static final int CODE_LEN = 4;
    public static final int SECRET_LEN = 9;

    // settings names
    public static final String ORGANIZER_CODE = "fs-conf-conference/organizer-code";
    public static final String PARTICIPANT_CODE = "fs-conf-conference/participant-code";
    public static final String REMOTE_ADMIT_SECRET = "fs-conf-conference/remote-admin-secret";
    public static final String AUTO_RECORDING = "fs-conf-conference/autorecord";
    public static final String AOR_RECORD = "fs-conf-conference/AOR";
    public static final String MAX_LEGS = "fs-conf-conference/MAX_LEGS";
    public static final String MOH = "fs-conf-conference/MOH";
    public static final String MOH_SOUNDCARD_SOURCE = "SOUNDCARD_SRC";

    private static final String ALIAS_RELATION = "conference";

    private boolean m_enabled;

    private String m_name;

    private String m_description;

    private String m_extension;

    private String m_did;

    private Bridge m_bridge;

    private User m_owner;

    private final ConferenceAorDefaults m_defaults;

    public Conference() {
        m_defaults = new ConferenceAorDefaults(this);
    }

    @Override
    public void initialize() {
        addDefaultBeanSettingHandler(m_defaults);
        getSettingModel2().setDefaultProfileNameHandler(new ConferenceProfileName(this));
    }

    @Override
    protected Setting loadSettings() {
        return getModelFilesContext().loadModelFile("sipxconference/conference.xml");
    }

    public String getDescription() {
        return m_description;
    }

    public void setDescription(String description) {
        m_description = description;
    }

    public boolean isEnabled() {
        return m_enabled;
    }

    public void setEnabled(boolean enabled) {
        m_enabled = enabled;
    }

    public String getName() {
        return m_name;
    }

    public void setName(String name) {
        m_name = name;
    }

    public Bridge getBridge() {
        return m_bridge;
    }

    public void setBridge(Bridge bridge) {
        m_bridge = bridge;
    }

    public User getOwner() {
        return m_owner;
    }

    public void setOwner(User owner) {
        m_owner = owner;
    }

    public boolean isAutorecorded() {
        if (m_owner == null) {
            // Can't auto-record if we don't have a conference owner.
            return false;
        }
        return (Boolean) getSettingTypedValue(AUTO_RECORDING);
    }

    public void setAutorecorded(boolean autorecord) {
        setSettingTypedValue(AUTO_RECORDING, autorecord);
    }

    public String getExtension() {
        return m_extension;
    }

    public void setExtension(String extension) {
        m_extension = extension;
    }

    public void generateAccessCodes() {
        m_defaults.generateAccessCodes();
    }

    public String getRemoteAdmitSecret() {
        return m_defaults.getRemoteAdmitSecret();
    }

    /**
     * It is called by deployment module every time we provision the bridge
     *
     */
    public void generateRemoteAdmitSecret() {
        m_defaults.generateRemoteAdmitSecret();
    }

    public String getOrganizerAccessCode() {
        return getSettingValue(ORGANIZER_CODE);
    }

    public String getParticipantAccessCode() {
        return getSettingValue(PARTICIPANT_CODE);
    }

    public boolean isMohPortAudioEnabled() {
        return getSettingValue(MOH).equals(MOH_SOUNDCARD_SOURCE);
    }

    public String getUri() {
        return getSettingValue(AOR_RECORD);
    }

    public boolean hasOwner() {
        return m_owner != null;
    }

    public Setting getConfigSettings() {
        return getSettings().getSetting("fs-conf-conference");
    }

    @Override
    public void setSettingValue(String path, String value) {
        if (AOR_RECORD.equals(path)) {
            throw new UnsupportedOperationException("cannot change AOR_RECORD");
        }
        super.setSettingValue(path, value);
    }

    public static class ConferenceAorDefaults {
        private final Conference m_conference;
        private String m_participantCode;
        private String m_organizerCode;
        private String m_remoteSecretAgent;

        ConferenceAorDefaults(Conference conference) {
            m_conference = conference;
        }

        void generateRemoteAdmitSecret() {
            m_remoteSecretAgent = RandomStringUtils.randomAlphanumeric(SECRET_LEN);
        }

        void generateAccessCodes() {
            m_organizerCode = RandomStringUtils.randomNumeric(CODE_LEN);
            m_participantCode = RandomStringUtils.randomNumeric(CODE_LEN);
        }

        @SettingEntry(path = AOR_RECORD)
        public String getAorRecord() {
            String user = m_conference.getName();
            String host = m_conference.getBridge().getHost();
            int port = m_conference.getBridge().getFreeswitchService().getFreeswitchSipPort();
            return SipUri.format((user == null) ? "" : user, host, port);
        }

        @SettingEntry(path = PARTICIPANT_CODE)
        public String getParticipantCode() {
            return m_participantCode;
        }

        @SettingEntry(path = ORGANIZER_CODE)
        public String getOrganizerCode() {
            return m_organizerCode;
        }

        @SettingEntry(path = REMOTE_ADMIT_SECRET)
        public String getRemoteAdmitSecret() {
            return m_remoteSecretAgent;
        }
    }

    public static class ConferenceProfileName implements ProfileNameHandler {
        private static final char SEPARATOR = '.';
        private final Conference m_conference;

        ConferenceProfileName(Conference conference) {
            m_conference = conference;
        }

        public SettingValue getProfileName(Setting setting) {
            String nameToken = SEPARATOR + m_conference.getName();
            String profileName = setting.getProfileName();
            StringBuffer buffer = new StringBuffer(profileName);
            int dotIndex = profileName.indexOf(SEPARATOR);
            if (dotIndex > 0) {
                buffer.insert(dotIndex, nameToken);
            } else {
                buffer.append(nameToken);
            }

            return new SettingValueImpl(buffer.toString());
        }
    }

    /**
     * Generates two alias mappings for a conference:
     *
     * extension@domain ==> name@domainm and name@domain ==>> media server
     *
     * @param domainName
     *
     * @return list of aliase mappings, empty list if conference is disabled
     */
    public List generateAliases(String domainName) {
        if (!isEnabled()) {
            return Collections.EMPTY_LIST;
        }
        ArrayList aliases = new ArrayList();
        if (StringUtils.isNotBlank(m_extension) && !m_extension.equals(m_name)) {
            // add extension mapping
            String extensionUri = AliasMapping.createUri(m_extension, domainName);
            String identityUri = SipUri.format(m_name, domainName, false);
            AliasMapping extensionAlias = new AliasMapping(extensionUri, identityUri, ALIAS_RELATION);
            aliases.add(extensionAlias);
        }
        if (StringUtils.isNotBlank(m_did) && !m_did.equals(m_name)) {
            // add extension mapping
            String didUri = AliasMapping.createUri(m_did, domainName);
            String identityUri = SipUri.format(m_name, domainName, false);
            AliasMapping didAlias = new AliasMapping(didUri, identityUri, ALIAS_RELATION);
            aliases.add(didAlias);
        }
        aliases.add(createFreeSwitchAlias(domainName));
        return aliases;
    }

    private AliasMapping createFreeSwitchAlias(String domainName) {
        String freeswitchUri = getUri();
        String identity = AliasMapping.createUri(m_name, domainName);
        return new AliasMapping(identity, freeswitchUri, ALIAS_RELATION);
    }

    public String getDid() {
        return m_did;
    }

    public void setDid(String did) {
        m_did = did;
    }
}
